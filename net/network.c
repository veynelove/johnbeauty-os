#include <net/network.h>

extern void printf(const char *str);

void network_init(network_stack_t *stack, jlos_amd_am79c973_t *eth0, uint32_t ip_be, uint32_t gateway_ip_be, uint32_t subnet_be)
{
    jlos_amd_am79c973_set_ip_address(eth0, ip_be);

    jlos_ether_frame_provider_init(&stack->etherframe, eth0);

    jlos_arp_init(&stack->arp, &stack->etherframe);

    jlos_internet_protocol_provider_init(&stack->ipv4, &stack->etherframe, &stack->arp, gateway_ip_be, subnet_be);

    jlos_icmp_init(&stack->icmp, &stack->ipv4);

    jlos_udp_provider_init(&stack->udp, &stack->ipv4);

    jlos_tcp_provider_init(&stack->tcp, &stack->ipv4);

    jlos_arp_broadcast_mac_address(&stack->arp, gateway_ip_be);
}
