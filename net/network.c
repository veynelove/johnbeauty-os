#include <net/network.h>
#include <tools/config.h>

extern void printf(const char *str);

void network_init(network_stack_t *stack, jlos_driver_manager_t *driver_manager_)
{
    jlos_amd_am79c973_t *eth0 = (jlos_amd_am79c973_t *)(driver_manager_->drivers[2]);
    
    uint32_t ip_be = BYTES_TO_BE32(144, 0x9F, 168, 192);
    uint32_t gateway_ip_be = BYTES_TO_BE32(1, 0x9F, 168, 192);
    uint32_t subnet_be = BYTES_TO_BE32(0, 255, 255, 255);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Setting IP to 192.168.159.144\n");
    #endif
    jlos_amd_am79c973_set_ip_address(eth0, ip_be);
    
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing Ethernet frame provider...\n");
    #endif
    jlos_ether_frame_provider_init(&stack->etherframe, eth0);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing ARP protocol...\n");
    #endif
    jlos_arp_init(&stack->arp, &stack->etherframe);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing IPv4 protocol (gateway: 192.168.159.1, subnet: 255.255.255.0)...\n");
    #endif
    jlos_internet_protocol_provider_init(&stack->ipv4, &stack->etherframe, &stack->arp, gateway_ip_be, subnet_be);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing ICMP protocol...\n");
    #endif
    jlos_icmp_init(&stack->icmp, &stack->ipv4);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing UDP protocol...\n");
    #endif
    jlos_udp_provider_init(&stack->udp, &stack->ipv4);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing TCP protocol...\n");
    #endif
    jlos_tcp_provider_init(&stack->tcp, &stack->ipv4);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Sending ARP broadcast to resolve gateway...\n");
    #endif
    jlos_arp_broadcast_mac_address(&stack->arp, gateway_ip_be);
    
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Network stack initialization complete!\n");
    #endif
}
