#ifndef _NETWORK_H
#define _NETWORK_H

#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/dhcp.h>
#include <net/dns.h>


typedef struct {
    uint32_t ip;
    uint32_t gateway;
    uint32_t subnet_mask;
    uint32_t dns_server;
} jlos_network_config_t;

typedef struct {
    jlos_network_config_t               config;
    jlos_ether_frame_provider_t         etherframe;
    jlos_arp_t                          arp;
    jlos_internet_protocol_provider_t   ipv4;
    jlos_icmp_t                         icmp;
    jlos_udp_provider_t                 udp;
    jlos_tcp_provider_t                 tcp;
    jlos_dhcp_client_t                  dhcp;
    jlos_dns_resolver_t                 dns;
} network_stack_t;

extern network_stack_t *g_network_stack;

void network_init(void);

void jlos_network_apply_config(void);

#endif
