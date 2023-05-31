#include <stdio.h>
#include <string.h>
#include "net/ipv6/addr.h"
#include "xtimer.h"
#include "thread.h"

#define STACKSIZE 2048
#define INTERVAL_SEC 10 // executa a cada 10 segundos

char lowest_latency_ip[IPV6_ADDR_MAX_STR_LEN]; // Variável global para armazenar o IP com a menor latência
int lowest_latency = -1; // Variável global para armazenar a menor latência

void find_ip_with_lowest_latency() {
    const char* IPV6_ADDRESSES[] = {
        "2001:660:3207:400::60",
        "2001:660:3207:400::61",
        // ...
        "2001:660:3207:400::70"
    };
    int SIZE = sizeof(IPV6_ADDRESSES) / sizeof(IPV6_ADDRESSES[0]);

    ipv6_addr_t ip;
    int latency;

    for (int i = 0; i < SIZE; ++i) {
        ipv6_addr_from_str(&ip, IPV6_ADDRESSES[i]);

        latency = get_latency(ip);

        if (latency >= 0 && (lowest_latency < 0 || latency < lowest_latency)) {
            lowest_latency = latency;
            strcpy(lowest_latency_ip, IPV6_ADDRESSES[i]);
        }
    }
}

void *thread_handler(void *arg) {
    (void)arg;

    while (1) {
        // Chama a função para encontrar o IP com menor latência.
        find_ip_with_lowest_latency();

        printf("O IP com a menor latência é: %s\n", lowest_latency_ip);
        printf("A menor latência é: %d\n", lowest_latency);

        // Aguarda por X segundos.
        xtimer_sleep(INTERVAL_SEC);
    }

    return NULL;
}

int main(void) {
    // Cria uma nova thread.
    thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1, 0,
                  thread_handler, NULL, "my_thread");

    return 0;
}
