#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shell.h"
#include "msg.h"
#include "net/emcute.h"
#include "net/ipv6/addr.h"
#include "thread.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net/netif.h" /* for resolving ipv6 scope */
#include "net/gnrc/rpl.h"
#include "net/gnrc/rpl/dodag.h"
#include "net/gnrc/rpl/structs.h"

#ifndef EMCUTE_ID
#define EMCUTE_ID ("gertrud")
#endif
#define EMCUTE_PRIO (THREAD_PRIORITY_MAIN - 1)

#define NUMOFSUBS (16U)
#define TOPIC_MAXLEN (64U)

static char stack[THREAD_STACKSIZE_DEFAULT];
static char client_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t queue[8];

static emcute_sub_t subscriptions[NUMOFSUBS];
static char topics[NUMOFSUBS][TOPIC_MAXLEN];

static void *emcute_thread(void *arg)
{
    (void)arg;
    emcute_run(CONFIG_EMCUTE_DEFAULT_PORT, EMCUTE_ID);
    return NULL; /* should never be reached */
}

static void on_pub(const emcute_topic_t *topic, void *data, size_t len)
{
    char *in = (char *)data;

    printf("### got publication for topic '%s' [%i] ###\n",
           topic->name, (int)topic->id);
    for (size_t i = 0; i < len; i++)
    {
        printf("%c", in[i]);
    }
    puts("");
}

static unsigned get_qos(const char *str)
{
    int qos = atoi(str);
    switch (qos)
    {
    case 1:
        return EMCUTE_QOS_1;
    case 2:
        return EMCUTE_QOS_2;
    default:
        return EMCUTE_QOS_0;
    }
}

static int cmd_con(int argc, char **argv)
{
    sock_udp_ep_t gw = {.family = AF_INET6, .port = CONFIG_EMCUTE_DEFAULT_PORT};
    char *topic = NULL;
    char *message = NULL;
    size_t len = 0;

    if (argc < 2)
    {
        printf("usage: %s <ipv6 addr> [port] [<will topic> <will message>]\n",
               argv[0]);
        return 1;
    }

    /* parse address */
    if (ipv6_addr_from_str((ipv6_addr_t *)&gw.addr.ipv6, argv[1]) == NULL)
    {
        printf("error parsing IPv6 address\n");
        return 1;
    }

    if (argc >= 3)
    {
        gw.port = atoi(argv[2]);
    }
    if (argc >= 5)
    {
        topic = argv[3];
        message = argv[4];
        len = strlen(message);
    }

    if (emcute_con(&gw, true, topic, message, len, 0) != EMCUTE_OK)
    {
        printf("error: unable to connect to [%s]:%i\n", argv[1], (int)gw.port);
        return 1;
    }
    printf("Successfully connected to gateway at [%s]:%i\n",
           argv[1], (int)gw.port);

    return 0;
}

static int cmd_discon(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int res = emcute_discon();
    if (res == EMCUTE_NOGW)
    {
        puts("error: not connected to any broker");
        return 1;
    }
    else if (res != EMCUTE_OK)
    {
        puts("error: unable to disconnect");
        return 1;
    }
    puts("Disconnect successful");
    return 0;
}

static int cmd_pub(int argc, char **argv)
{
    emcute_topic_t t;
    unsigned flags = EMCUTE_QOS_0;

    if (argc < 3)
    {
        printf("usage: %s <topic name> <data> [QoS level]\n", argv[0]);
        return 1;
    }

    /* parse QoS level */
    if (argc >= 4)
    {
        flags |= get_qos(argv[3]);
    }

    printf("pub with topic: %s and name %s and flags 0x%02x\n", argv[1], argv[2], (int)flags);

    /* step 1: get topic id */
    t.name = argv[1];
    if (emcute_reg(&t) != EMCUTE_OK)
    {
        puts("error: unable to obtain topic ID");
        return 1;
    }

    /* step 2: publish data */
    if (emcute_pub(&t, argv[2], strlen(argv[2]), flags) != EMCUTE_OK)
    {
        printf("error: unable to publish data to topic '%s [%i]'\n",
               t.name, (int)t.id);
        return 1;
    }

    printf("Published %i bytes to topic '%s [%i]'\n",
           (int)strlen(argv[2]), t.name, t.id);

    return 0;
}

static int cmd_sub(int argc, char **argv)
{
    unsigned flags = EMCUTE_QOS_0;

    if (argc < 2)
    {
        printf("usage: %s <topic name> [QoS level]\n", argv[0]);
        return 1;
    }

    if (strlen(argv[1]) > TOPIC_MAXLEN)
    {
        puts("error: topic name exceeds maximum possible size");
        return 1;
    }
    if (argc >= 3)
    {
        flags |= get_qos(argv[2]);
    }

    /* find empty subscription slot */
    unsigned i = 0;
    for (; (i < NUMOFSUBS) && (subscriptions[i].topic.id != 0); i++)
    {
    }
    if (i == NUMOFSUBS)
    {
        puts("error: no memory to store new subscriptions");
        return 1;
    }

    subscriptions[i].cb = on_pub;
    strcpy(topics[i], argv[1]);
    subscriptions[i].topic.name = topics[i];
    if (emcute_sub(&subscriptions[i], flags) != EMCUTE_OK)
    {
        printf("error: unable to subscribe to %s\n", argv[1]);
        return 1;
    }

    printf("Now subscribed to %s\n", argv[1]);
    return 0;
}

static int cmd_unsub(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage %s <topic name>\n", argv[0]);
        return 1;
    }

    /* find subscriptions entry */
    for (unsigned i = 0; i < NUMOFSUBS; i++)
    {
        if (subscriptions[i].topic.name &&
            (strcmp(subscriptions[i].topic.name, argv[1]) == 0))
        {
            if (emcute_unsub(&subscriptions[i]) == EMCUTE_OK)
            {
                memset(&subscriptions[i], 0, sizeof(emcute_sub_t));
                printf("Unsubscribed from '%s'\n", argv[1]);
            }
            else
            {
                printf("Unsubscription form '%s' failed\n", argv[1]);
            }
            return 0;
        }
    }

    printf("error: no subscription for topic '%s' found\n", argv[1]);
    return 1;
}

static int cmd_will(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("usage %s <will topic name> <will message content>\n", argv[0]);
        return 1;
    }

    if (emcute_willupd_topic(argv[1], 0) != EMCUTE_OK)
    {
        puts("error: unable to update the last will topic");
        return 1;
    }
    if (emcute_willupd_msg(argv[2], strlen(argv[2])) != EMCUTE_OK)
    {
        puts("error: unable to update the last will message");
        return 1;
    }

    puts("Successfully updated last will topic and message");
    return 0;
}

static const shell_command_t shell_commands[] = {
    {"con", "connect to MQTT broker", cmd_con},
    {"discon", "disconnect from the current broker", cmd_discon},
    {"pub", "publish something", cmd_pub},
    {"sub", "subscribe topic", cmd_sub},
    {"unsub", "unsubscribe from topic", cmd_unsub},
    {"will", "register a last will", cmd_will},
    {NULL, NULL, NULL}};

#define CLIENT_BUFFER_SIZE (128)
static char client_buffer[CLIENT_BUFFER_SIZE];

#if 0
static int udp_send(char *addr_str, uint16_t port, char *data)
{
    struct sockaddr_in6 src, dst;
    size_t data_len = strlen(data);
    int s;
    src.sin6_family = AF_INET6;
    dst.sin6_family = AF_INET6;
    memset(&src.sin6_addr, 0, sizeof(src.sin6_addr));
    /* parse interface id */
#ifdef SOCK_HAS_IPV6
    char *iface;
    iface = ipv6_addr_split_iface(addr_str); /* also removes interface id */
    if (iface) {
        netif_t *netif = netif_get_by_name(iface);
        if (netif) {
            dst.sin6_scope_id = (uint32_t) netif_get_id(netif);
        }
        else {
            printf("unknown network interface %s\n", iface);
        }
    }
#endif /* SOCK_HAS_IPV6 */
    /* parse destination address */
    if (inet_pton(AF_INET6, addr_str, &dst.sin6_addr) != 1) {
        puts("Error: unable to parse destination address");
        return 1;
    }

    dst.sin6_port = htons(port);
    src.sin6_port = htons(port);
    s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        puts("error initializing socket");
        return 1;
    }
    if (sendto(s, data, data_len, 0, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        puts("could not send");
    }
    else {
	printf("Successfully sending, waiting response...\n");
	int res;
	socklen_t dst_len = sizeof(struct sockaddr_in6);
	if ((res = recvfrom(s, client_buffer, sizeof(client_buffer), 0,
                            (struct sockaddr *)&dst, &dst_len)) < 0) {
        	puts("Error on receive");
        }
        else if (res == 0) {
            puts("Peer did shut down");
        }
        else {
            printf("Received data: ");
            puts(client_buffer);
        }
    }

    close(s);
    return 0;
}

#define CLIENT_MSG_QUEUE_SIZE (8)
static msg_t client_msg_queue[CLIENT_MSG_QUEUE_SIZE];

static void *client_thread(void *args)
{
	(void)args;
	msg_init_queue(client_msg_queue, CLIENT_MSG_QUEUE_SIZE);
	kernel_pid_t iface_pid = 6;
	if (gnrc_netif_get_by_pid(iface_pid) == NULL) {
        	printf("unknown interface specified\n");
       		return NULL;
    	}

	gnrc_rpl_init(iface_pid);
	printf("successfully initialized RPL on interface %d\n", iface_pid);

        while(1){
		sleep(5);
                (void)udp_send("2001:db8::1", 8000, "gateway_ipv6_request");
        }
	return NULL;
}
#endif

static void *client_thread(void *args)
{
    (void)args;

    kernel_pid_t iface_pid = 6;
    if (gnrc_netif_get_by_pid(iface_pid) == NULL)
    {
        printf("unknown interface specified\n");
        return NULL;
    }

    gnrc_rpl_init(iface_pid);
    printf("successfully initialized RPL on interface %d\n", iface_pid);

    sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
    sock_udp_t sock;

    local.port = 0xabcd;

    if (sock_udp_create(&sock, &local, NULL, 0) < 0)
    {
        puts("Error creating UDP sock");
        return NULL;
    }

    sock_udp_ep_t remote = {.family = AF_INET6};
    ssize_t res;

    remote.port = 8000;
    // Parsing IPv6 Address from String
    if (ipv6_addr_from_str((ipv6_addr_t *)&remote.addr.ipv6, "2001:db8::1") == NULL)
    {
        printf("error parsing IPv6 address\n");
        return NULL;
    }

    while (1)
    {
        sleep(5);
        uint8_t instance_id = 1; // O ID da instância que você deseja obter.

        gnrc_rpl_instance_t *instance = gnrc_rpl_instance_get(instance_id);
        if (instance != NULL)
        {
            // Buffer para armazenar a string do endereço IPv6.
            char addr_str[IPV6_ADDR_MAX_STR_LEN];

            // Converte o endereço IPv6 para uma string.
            ipv6_addr_to_str(addr_str, &(instance->dodag.dodag_id), IPV6_ADDR_MAX_STR_LEN);

            // Imprime o endereço IPv6.
            printf("DODAG IPv6 address: %s\n", addr_str);
        }
        else
        {
            printf("Instance or DODAG not found.\n");
        }

        if (sock_udp_send(&sock, "gateway_ipv6_request", sizeof("gateway_ipv6_request"), &remote) < 0)
        {
            puts("Error sending message");
        }
        else
        {
            printf("Successfully sending, waiting response...\n");
            if ((res = sock_udp_recv(&sock, client_buffer, sizeof(client_buffer), 1 * US_PER_SEC,
                                     NULL)) < 0)
            {
                if (res == -ETIMEDOUT)
                {
                    puts("Timed out");
                }
                else
                {
                    puts("Error receiving message");
                }
            }
            else
            {
                printf("Received data: ");
                puts(client_buffer);
            }
        }
    }
    return NULL;
}

int main(void)
{
    puts("MQTT-SN example application\n");
    puts("Type 'help' to get started. Have a look at the README.md for more"
         "information.");

    /* the main thread needs a msg queue to be able to run `ping`*/
    msg_init_queue(queue, ARRAY_SIZE(queue));

    /* initialize our subscription buffers */
    memset(subscriptions, 0, (NUMOFSUBS * sizeof(emcute_sub_t)));

    /* start the emcute thread */
    thread_create(stack, sizeof(stack), EMCUTE_PRIO, 0,
                  emcute_thread, NULL, "emcute");

    if (thread_create(client_stack, sizeof(client_stack), THREAD_PRIORITY_MAIN - 1,
                      THREAD_CREATE_STACKTEST,
                      client_thread, NULL, "UDP client") <= KERNEL_PID_UNDEF)
    {
        puts("error initializing thread");
    }

    /* start shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
