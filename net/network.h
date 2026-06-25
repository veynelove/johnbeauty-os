#ifndef __NETWORK_H
#define __NETWORK_H

#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>

typedef struct {
    jlos_ether_frame_provider_t etherframe;
    jlos_arp_t arp;
    jlos_internet_protocol_provider_t ipv4;
    jlos_icmp_t icmp;
    jlos_udp_provider_t udp;
    jlos_tcp_provider_t tcp;
} network_stack_t;

void network_init(network_stack_t *stack, jlos_amd_am79c973_t *eth0, uint32_t ip_be, uint32_t gateway_ip_be, uint32_t subnet_be);

#endif
