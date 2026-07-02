#include <net/network.h>
#include <tools/config.h>
#include <kernel/printk.h>

void network_init(network_stack_t *stack, jlos_driver_manager_t *driver_manager_)
{
    /* 遍历所有驱动查找 AMD am79c973 网卡驱动（硬编码 index[2] 容易出问题） */
    extern void jlos_amd_am79c973_activate(jlos_amd_am79c973_t*);
    jlos_amd_am79c973_t *eth0 = NULL;
    for (int i = 0; i < driver_manager_->m_num_drivers; i++) {
        jlos_driver_t *drv = driver_manager_->drivers[i];
        if (drv != NULL && drv->activate == (void (*)(jlos_driver_t*))jlos_amd_am79c973_activate) {
            eth0 = (jlos_amd_am79c973_t *)drv;
            break;
        }
    }
    if (eth0 == NULL) {
        printf("WARNING: AMD am79c973 ethernet driver not found, skipping network stack init\n");
        return;  /* 找不到网卡直接安全返回，不要让后面的测试代码跑不起来 */
    }
    
    uint32_t ip_be = BYTES_TO_BE32(144, 159, 168, 192);
    uint32_t gateway_ip_be = BYTES_TO_BE32(1, 0x9F, 168, 192);
    uint32_t subnet_be = BYTES_TO_BE32(0, 255, 255, 255);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("Initializing network stack...\n");
    printf("NET: Setting IP to 192.168.159.144\n");
    #endif
    jlos_amd_am79c973_set_ip_address(eth0, ip_be);
    
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing Ethernet frame provider...\n");
    #endif
    jlos_ether_frame_provider_init(&stack->etherframe, eth0);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    bool ether_ok = true;
    if (stack->etherframe.handlers == NULL) {
        printf("NET: [FAIL] EtherFrame handlers allocation failed!\n");
        ether_ok = false;
    }
    if (stack->etherframe.base_handler.backend == NULL) {
        printf("NET: [FAIL] EtherFrame backend is NULL!\n");
        ether_ok = false;
    }
    if (stack->etherframe.base_handler.on_raw_data_received == NULL) {
        printf("NET: [FAIL] EtherFrame raw data handler is NULL!\n");
        ether_ok = false;
    }
    if (ether_ok) {
        printf("NET: [ OK ] EtherFrame initialized (handlers=0x");
        printf_hex32((uint32_t)stack->etherframe.handlers);
        printf(")\n");
    }
    #endif

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing ARP protocol...\n");
    #endif
    jlos_arp_init(&stack->arp, &stack->etherframe);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    bool arp_ok = true;
    uint16_t arp_etype = JLOS_SWAP_ENDIAN_16(0x806);
    if (stack->arp.base_handler.backend == NULL) {
        printf("ARP: [FAIL] backend is NULL!\n");
        arp_ok = false;
    }
    if (stack->arp.base_handler.m_etherType_BE != arp_etype) {
        printf("ARP: [FAIL] EtherType mismatch! expected=0x");
        printf_hex16(arp_etype);
        printf(" actual=0x");
        printf_hex16(stack->arp.base_handler.m_etherType_BE);
        printf("\n");
        arp_ok = false;
    }
    if (stack->arp.base_handler.on_ether_frame_received == NULL) {
        printf("ARP: [FAIL] frame handler is NULL!\n");
        arp_ok = false;
    }
    if (stack->arp.m_num_cache_entries != 0) {
        printf("ARP: [FAIL] cache entries should be 0, actual=");
        printf_hex32(stack->arp.m_num_cache_entries);
        printf("\n");
        arp_ok = false;
    }
    if (stack->etherframe.handlers[arp_etype] != &stack->arp.base_handler) {
        printf("ARP: [FAIL] not registered in EtherFrame handlers!\n");
        arp_ok = false;
    }
    if (arp_ok) {
        printf("ARP: [ OK ] initialized (type=0x0806, cache_size=128)\n");
    }
    #endif

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing IPv4 protocol (gateway: 192.168.159.1, subnet: 255.255.255.0)...\n");
    #endif
    jlos_internet_protocol_provider_init(&stack->ipv4, &stack->etherframe, &stack->arp, gateway_ip_be, subnet_be);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    bool ipv4_ok = true;
    uint16_t ip_etype = JLOS_SWAP_ENDIAN_16(0x800);
    if (stack->ipv4.base_handler.backend == NULL) {
        printf("IP4: [FAIL] backend is NULL!\n");
        ipv4_ok = false;
    }
    if (stack->ipv4.base_handler.m_etherType_BE != ip_etype) {
        printf("IP4: [FAIL] EtherType mismatch! expected=0x");
        printf_hex16(ip_etype);
        printf(" actual=0x");
        printf_hex16(stack->ipv4.base_handler.m_etherType_BE);
        printf("\n");
        ipv4_ok = false;
    }
    if (stack->ipv4.base_handler.on_ether_frame_received == NULL) {
        printf("IP4: [FAIL] frame handler is NULL!\n");
        ipv4_ok = false;
    }
    if (stack->ipv4.arp == NULL) {
        printf("IP4: [FAIL] ARP reference is NULL!\n");
        ipv4_ok = false;
    }
    if (stack->ipv4.m_gateway_ip != gateway_ip_be) {
        printf("IP4: [FAIL] gateway IP mismatch!\n");
        ipv4_ok = false;
    }
    if (stack->ipv4.m_subnet_mask != subnet_be) {
        printf("IP4: [FAIL] subnet mask mismatch!\n");
        ipv4_ok = false;
    }
    if (stack->etherframe.handlers[ip_etype] != &stack->ipv4.base_handler) {
        printf("IP4: [FAIL] not registered in EtherFrame handlers!\n");
        ipv4_ok = false;
    }
    if (ipv4_ok) {
        printf("IP4: [ OK ] initialized (type=0x0800, gw=0x");
        printf_hex32(stack->ipv4.m_gateway_ip);
        printf(" mask=0x");
        printf_hex32(stack->ipv4.m_subnet_mask);
        printf(")\n");
    }
    #endif

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing ICMP protocol...\n");
    #endif
    jlos_icmp_init(&stack->icmp, &stack->ipv4);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    bool icmp_ok = true;
    if (stack->icmp.base_handler.backend == NULL) {
        printf("ICMP: [FAIL] backend is NULL!\n");
        icmp_ok = false;
    }
    if (stack->icmp.base_handler.m_ip_protocol != 0x01) {
        printf("ICMP: [FAIL] protocol mismatch! expected=0x01 actual=0x");
        printf_hex(stack->icmp.base_handler.m_ip_protocol);
        printf("\n");
        icmp_ok = false;
    }
    if (stack->icmp.base_handler.on_internet_protocol_received == NULL) {
        printf("ICMP: [FAIL] protocol handler is NULL!\n");
        icmp_ok = false;
    }
    if (stack->ipv4.handlers[0x01] != &stack->icmp.base_handler) {
        printf("ICMP: [FAIL] not registered in IPv4 handlers!\n");
        icmp_ok = false;
    }
    if (icmp_ok) {
        printf("ICMP: [ OK ] initialized (proto=0x01)\n");
    }
    #endif

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing UDP protocol...\n");
    #endif
    jlos_udp_provider_init(&stack->udp, &stack->ipv4);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    bool udp_ok = true;
    if (stack->udp.base_handler.backend == NULL) {
        printf("UDP: [FAIL] backend is NULL!\n");
        udp_ok = false;
    }
    if (stack->udp.base_handler.m_ip_protocol != 0x11) {
        printf("UDP: [FAIL] protocol mismatch! expected=0x11 actual=0x");
        printf_hex(stack->udp.base_handler.m_ip_protocol);
        printf("\n");
        udp_ok = false;
    }
    if (stack->udp.base_handler.on_internet_protocol_received == NULL) {
        printf("UDP: [FAIL] protocol handler is NULL!\n");
        udp_ok = false;
    }
    if (stack->udp.sockets == NULL) {
        printf("UDP: [FAIL] sockets array allocation failed!\n");
        udp_ok = false;
    }
    if (stack->udp.m_num_sockets != 0) {
        printf("UDP: [FAIL] num_sockets should be 0, actual=");
        printf_hex16(stack->udp.m_num_sockets);
        printf("\n");
        udp_ok = false;
    }
    if (stack->udp.m_free_port != 1024) {
        printf("UDP: [FAIL] free_port should be 1024, actual=");
        printf_hex16(stack->udp.m_free_port);
        printf("\n");
        udp_ok = false;
    }
    if (stack->ipv4.handlers[0x11] != &stack->udp.base_handler) {
        printf("UDP: [FAIL] not registered in IPv4 handlers!\n");
        udp_ok = false;
    }
    if (udp_ok) {
        printf("UDP: [ OK ] initialized (proto=0x11, sockets=0x");
        printf_hex32((uint32_t)stack->udp.sockets);
        printf(")\n");
    }
    #endif

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Initializing TCP protocol...\n");
    #endif
    jlos_tcp_provider_init(&stack->tcp, &stack->ipv4);

    #if KERNEL_CONFIG_DEBUG_NETWORK
    bool tcp_ok = true;
    if (stack->tcp.base_handler.backend == NULL) {
        printf("TCP: [FAIL] backend is NULL!\n");
        tcp_ok = false;
    }
    if (stack->tcp.base_handler.m_ip_protocol != 0x06) {
        printf("TCP: [FAIL] protocol mismatch! expected=0x06 actual=0x");
        printf_hex(stack->tcp.base_handler.m_ip_protocol);
        printf("\n");
        tcp_ok = false;
    }
    if (stack->tcp.base_handler.on_internet_protocol_received == NULL) {
        printf("TCP: [FAIL] protocol handler is NULL!\n");
        tcp_ok = false;
    }
    if (stack->tcp.sockets == NULL) {
        printf("TCP: [FAIL] sockets array allocation failed!\n");
        tcp_ok = false;
    }
    if (stack->tcp.m_num_sockets != 0) {
        printf("TCP: [FAIL] num_sockets should be 0, actual=");
        printf_hex16(stack->tcp.m_num_sockets);
        printf("\n");
        tcp_ok = false;
    }
    if (stack->tcp.m_free_port != 1024) {
        printf("TCP: [FAIL] free_port should be 1024, actual=");
        printf_hex16(stack->tcp.m_free_port);
        printf("\n");
        tcp_ok = false;
    }
    if (stack->ipv4.handlers[0x06] != &stack->tcp.base_handler) {
        printf("TCP: [FAIL] not registered in IPv4 handlers!\n");
        tcp_ok = false;
    }
    if (tcp_ok) {
        printf("TCP: [ OK ] initialized (proto=0x06, sockets=0x");
        printf_hex32((uint32_t)stack->tcp.sockets);
        printf(")\n");
    }
    #endif

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Sending ARP broadcast to resolve gateway...\n");
    #endif
    jlos_arp_broadcast_mac_address(&stack->arp, gateway_ip_be);
    
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("NET: Network stack initialization complete!\n");
    #endif
}
