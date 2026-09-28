#ifndef _JLOS_NET_DHCP_H
#define _JLOS_NET_DHCP_H

#include <net/udp.h>

#define JLOS_DHCP_CLIENT_PORT       68
#define JLOS_DHCP_SERVER_PORT       67

#define JLOS_DHCP_OP_BOOTREQUEST    1
#define JLOS_DHCP_OP_BOOTREPLY      2

#define JLOS_DHCP_HTYPE_ETHERNET    1
#define JLOS_DHCP_HLEN_ETHERNET     6

#define JLOS_DHCP_FLAGS_BROADCAST   0x8000

#define JLOS_DHCP_MAGIC_COOKIE      0x63825363

#define JLOS_DHCP_OPT_PAD           0
#define JLOS_DHCP_OPT_SUBNET_MASK   1
#define JLOS_DHCP_OPT_ROUTER        3
#define JLOS_DHCP_OPT_DNS_SERVER    6
#define JLOS_DHCP_OPT_REQUESTED_IP  50
#define JLOS_DHCP_OPT_SERVER_ID     54
#define JLOS_DHCP_OPT_MSG_TYPE      53
#define JLOS_DHCP_OPT_END           255

#define JLOS_DHCP_MSG_DISCOVER      1
#define JLOS_DHCP_MSG_OFFER         2
#define JLOS_DHCP_MSG_REQUEST       3
#define JLOS_DHCP_MSG_ACK           5
#define JLOS_DHCP_MSG_NAK           6

#define JLOS_DHCP_OPTIONS_MAX       64
#define JLOS_DHCP_MIN_PACKET        300

typedef struct {
    uint8_t     op;
    uint8_t     htype;
    uint8_t     hlen;
    uint8_t     hops;
    uint32_t    xid;
    uint16_t    secs;
    uint16_t    flags;
    uint32_t    ciaddr;
    uint32_t    yiaddr;
    uint32_t    siaddr;
    uint32_t    giaddr;
    uint8_t     chaddr[16];
    uint8_t     sname[64];
    uint8_t     file[128];
    uint32_t    magic_cookie;
} __attribute__((packed)) jlos_dhcp_header_t;

typedef enum {
    JLOS_DHCP_STATE_INIT,
    JLOS_DHCP_STATE_SELECTING,
    JLOS_DHCP_STATE_REQUESTING,
    JLOS_DHCP_STATE_BOUND,
    JLOS_DHCP_STATE_FAILED,
} jlos_dhcp_state_t;

typedef struct {
    jlos_udp_handler_t   handler;
    jlos_udp_socket_t   *socket;
    jlos_udp_provider_t *udp;
    jlos_dhcp_state_t   state;
    uint32_t            xid;
    uint32_t            server_id;
    uint32_t            offered_ip;
    uint8_t             mac[6];
} jlos_dhcp_client_t;

void jlos_dhcp_client_init(jlos_dhcp_client_t *self, jlos_udp_provider_t *udp, uint8_t mac[6]);
void jlos_dhcp_client_discover(jlos_dhcp_client_t *self);

#endif
