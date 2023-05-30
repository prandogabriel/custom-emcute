# name of your application
APPLICATION = emcute_mqttsn

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Include packages that pull up and auto-init the link layer.
# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
# Activate ICMPv6 error messages
USEMODULE += gnrc_icmpv6_error
# Specify the mandatory networking module for a IPv6 routing node
USEMODULE += gnrc_ipv6_router_default
# Add a routing protocol
USEMODULE += gnrc_rpl
USEMODULE += auto_init_gnrc_rpl

# RPL config
# Exclude Prefix Information Options from DIOs
CFLAGS += -DCONFIG_GNRC_RPL_WITHOUT_PIO
# Modify trickle parameters
CFLAGS += -DCONFIG_GNRC_RPL_DEFAULT_DIO_INTERVAL_DOUBLINGS=20
CFLAGS += -DCONFIG_GNRC_RPL_DEFAULT_DIO_INTERVAL_MIN=3
CFLAGS += -DCONFIG_GNRC_RPL_DEFAULT_DIO_REDUNDANCY_CONSTANT=10
# Make reception of DODAG_CONF optional when joining a DODAG. This will use the default trickle parameters until a DODAG_CONF is received from the parent. The DODAG_CONF is requested once from the parent while joining the DODAG. The standard behaviour is to request a DODAG_CONF and join only a DODAG once a DODAG_CONF is received.
CFLAGS += -DCONFIG_GNRC_RPL_DODAG_CONF_OPTIONAL_ON_JOIN
# Set interface for auto-initialization if more than one interface exists (gnrc_netif_highlander() returns false)
CFLAGS += -DCONFIG_GNRC_RPL_DEFAULT_NETIF=6
# By default, all incoming control messages get checked for validation. This validation can be disabled in case the involved RPL implementations are known to produce valid messages.
CFLAGS += -DCONFIG_GNRC_RPL_WITHOUT_VALIDATION
# This RPL implementation currently only supports storing mode. That means, in order to have downwards routes to all nodes the storage space within gnrc_ipv6's Neighbor Information Base must be big enough to store information for each node.
# For a random topology of n nodes, to ensure you can reach every node from the root, set CONFIG_GNRC_IPV6_NIB_NUMOF == CONFIG_GNRC_IPV6_NIB_OFFL_NUMOF == n.
CFLAGS += -DCONFIG_GNRC_IPV6_NIB_NUMOF=50
CFLAGS += -DCONFIG_GNRC_IPV6_NIB_OFFL_NUMOF=50
# If you want to allow for alternative parents, increase the number of default routers in the NIB.
CFLAGS += -DCONFIG_GNRC_IPV6_NIB_DEFAULT_ROUTER_NUMOF=2

# Include MQTT-SN
USEMODULE += emcute
# Add also the shell, some shell commands
USEMODULE += sock_udp
USEMODULE += posix_sockets
USEMODULE += posix_sleep
USEMODULE += posix_inet
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps
USEMODULE += netstats_l2
USEMODULE += netstats_ipv6
USEMODULE += netstats_rpl
# For testing we also include the ping command and some stats
USEMODULE += gnrc_icmpv6_echo
USEMODULE += shell_cmd_gnrc_udp

CFLAGS += -DCONFIG_GNRC_NETIF_IPV6_ADDRS_NUMOF=4

# Allow for env-var-based override of the nodes name (EMCUTE_ID)
ifneq (,$(EMCUTE_ID))
  CFLAGS += -DEMCUTE_ID=\"$(EMCUTE_ID)\"
endif

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Comment this out to join RPL DODAGs even if DIOs do not contain
# DODAG Configuration Options (see the doc for more info)
# CFLAGS += -DCONFIG_GNRC_RPL_DODAG_CONF_OPTIONAL_ON_JOIN

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
