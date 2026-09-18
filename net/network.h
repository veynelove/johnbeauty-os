#ifndef _NETWORK_H
#define _NETWORK_H

#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>

typedef struct {
    jlos_ether_frame_provider_t         etherframe;
    jlos_arp_t                          arp;
    jlos_internet_protocol_provider_t   ipv4;
    jlos_icmp_t                         icmp;
    jlos_udp_provider_t                 udp;
    jlos_tcp_provider_t                 tcp;
} network_stack_t;

extern network_stack_t *g_network_stack;

void network_init(void);

#endif
