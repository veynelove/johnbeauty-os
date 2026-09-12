#include <net/network.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <tools/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "net"

extern void jlos_amd_am79c973_activate(jlos_amd_am79c973_t*);

network_stack_t *g_network_stack = NULL;

void network_init(void)
{
    /* 遍历所有驱动查找 AMD AM79C973 网卡驱动（硬编码 index[2] 容易出问题） */
    g_network_stack = (network_stack_t *)jlos_kalloc(sizeof(network_stack_t));
    if (!g_network_stack) {
        printk_err("network init failed. g_network_stack is NULL\n");
        return;
    }

    jlos_amd_am79c973_t *eth0 = NULL;
    for (int i = 0; i < g_driver_manager_ptr->num_drivers; i++) {
        jlos_driver_t *drv = g_driver_manager_ptr->drivers[i];
        if (drv != NULL && drv->activate == (void (*)(jlos_driver_t*))jlos_amd_am79c973_activate) {
            eth0 = (jlos_amd_am79c973_t *)drv;
            break;
        }
    }
    if (eth0 == NULL) {
        printk_warn("AMD AM79C973 ethernet driver not found, skipping network stack init\n");
        return;
    }
    uint32_t ip_be = BYTES_TO_BE32(144, 159, 168, 192);
    uint32_t gateway_ip_be = BYTES_TO_BE32(1, 0x9F, 168, 192);
    uint32_t subnet_be = BYTES_TO_BE32(0, 255, 255, 255);
    printk_info("initializing network stack\n");
    printk_info("setting IP to 192.168.159.144\n");
    jlos_amd_am79c973_set_ip_address(eth0, ip_be);

    printk_info("initializing etherframe provider\n");
    jlos_ether_frame_provider_init(&g_network_stack->etherframe, eth0);
#if KERNEL_CONFIG_DEBUG_NETWORK
    bool ether_ok = true;
    if (g_network_stack->etherframe.handlers.buckets == NULL) {
        printk_err("[FAIL] etherframe handlers allocation failed\n");
        ether_ok = false;
    }
    if (g_network_stack->etherframe.base_handler.backend == NULL) {
        printk_err("[FAIL] etherframe backend is NULL\n");
        ether_ok = false;
    }
    if (g_network_stack->etherframe.base_handler.on_raw_data_received == NULL) {
        printk_err("[FAIL] etherframe raw data handler is NULL\n");
        ether_ok = false;
    }
    if (ether_ok) {
        printk_info("[OK] etherframe initialized (handlers=%p)\n", g_network_stack->etherframe.handlers.buckets);
    }
#endif

    jlos_arp_init(&g_network_stack->arp, &g_network_stack->etherframe);
#if KERNEL_CONFIG_DEBUG_NETWORK
    bool arp_ok = true;
    uint16_t arp_etype = JLOS_SWAP_ENDIAN_16(0x806);
    if (g_network_stack->arp.base_handler.backend == NULL) {
        printk_err("[FAIL] ARP backend is NULL\n");
        arp_ok = false;
    }
    if (g_network_stack->arp.base_handler.etherType_BE != arp_etype) {
        printk_err("[FAIL] ARP ethertype mismatch, expected=0x%x actual=0x%x\n",
            arp_etype, g_network_stack->arp.base_handler.etherType_BE);
        arp_ok = false;
    }
    if (g_network_stack->arp.base_handler.on_ether_frame_received == NULL) {
        printk_err("[FAIL] ARP frame handler is NULL\n");
        arp_ok = false;
    }
    if (g_network_stack->arp.num_cache_entries != 0) {
        printk_err("[FAIL] ARP cache entries should be 0, actual=%x\n", g_network_stack->arp.num_cache_entries);
        arp_ok = false;
    }
    jlos_hash_node_t *ether_node = jlos_hash_chain_see(&g_network_stack->etherframe.handlers, &arp_etype);
    jlos_ether_frame_handler_t *ether_handler = container_of(ether_node, jlos_ether_frame_handler_t, hash_node);
    if (ether_handler != &g_network_stack->arp.base_handler) {
        printk_err("[FAIL] ARP not registered in etherframe handlers\n");
        arp_ok = false;
    }
    if (arp_ok) {
        printk_info("[OK] ARP initialized (type=0x0806, cache_size=128)\n");
    }
#endif

    printk_info("initializing IPv4 protocol (gateway: 192.168.159.1, subnet: 255.255.255.0)\n");
    jlos_internet_protocol_provider_init(&g_network_stack->ipv4, &g_network_stack->etherframe, &g_network_stack->arp, gateway_ip_be, subnet_be);
#if KERNEL_CONFIG_DEBUG_NETWORK
    bool ipv4_ok = true;
    uint16_t ip_etype = JLOS_SWAP_ENDIAN_16(0x800);
    if (g_network_stack->ipv4.base_handler.backend == NULL) {
        printk_err("[FAIL] IPv4 backend is NULL\n");
        ipv4_ok = false;
    }
    if (g_network_stack->ipv4.base_handler.etherType_BE != ip_etype) {
        printk_err("[FAIL] IPv4 ethertype mismatch, expected=0x%x actual=0x%x\n",
            ip_etype, g_network_stack->ipv4.base_handler.etherType_BE);
        ipv4_ok = false;
    }
    if (g_network_stack->ipv4.base_handler.on_ether_frame_received == NULL) {
        printk_err("[FAIL] IPv4 frame handler is NULL\n");
        ipv4_ok = false;
    }
    if (g_network_stack->ipv4.arp == NULL) {
        printk_err("[FAIL] IPv4 ARP reference is NULL\n");
        ipv4_ok = false;
    }
    if (g_network_stack->ipv4.gateway_ip != gateway_ip_be) {
        printk_err("[FAIL] IPv4 gateway IP mismatch\n");
        ipv4_ok = false;
    }
    if (g_network_stack->ipv4.subnet_mask != subnet_be) {
        printk_err("[FAIL] IPv4 subnet mask mismatch\n");
        ipv4_ok = false;
    }
    jlos_hash_node_t *ip_node = jlos_hash_chain_see(&g_network_stack->etherframe.handlers, &ip_etype);
    jlos_ether_frame_handler_t *ip_handler = container_of(ip_node, jlos_ether_frame_handler_t, hash_node);
    if (ip_handler != &g_network_stack->ipv4.base_handler) {
        printk_err("[FAIL] IPv4 not registered in etherframe handlers\n");
        ipv4_ok = false;
    }
    if (ipv4_ok) {
        printk_info("[OK] IPv4 initialized (type=0x0800, gw=0x%x mask=0x%x)\n",
            g_network_stack->ipv4.gateway_ip, g_network_stack->ipv4.subnet_mask);
    }
#endif

    printk_info("initializing ICMP protocol\n");
    jlos_icmp_init(&g_network_stack->icmp, &g_network_stack->ipv4);
#if KERNEL_CONFIG_DEBUG_NETWORK
    bool icmp_ok = true;
    if (g_network_stack->icmp.base_handler.backend == NULL) {
        printk_err("[FAIL] ICMP backend is NULL\n");
        icmp_ok = false;
    }
    if (g_network_stack->icmp.base_handler.ip_protocol != 0x01) {
        printk_err("[FAIL] ICMP protocol mismatch, expected=0x01 actual=0x%x\n",
            g_network_stack->icmp.base_handler.ip_protocol);
        icmp_ok = false;
    }
    if (g_network_stack->icmp.base_handler.on_internet_protocol_received == NULL) {
        printk_err("[FAIL] ICMP protocol handler is NULL\n");
        icmp_ok = false;
    }
    if (g_network_stack->ipv4.handlers[0x01] != &g_network_stack->icmp.base_handler) {
        printk_err("[FAIL] ICMP not registered in IPv4 handlers\n");
        icmp_ok = false;
    }
    if (icmp_ok) {
        printk_info("[OK] ICMP initialized (proto=0x01)\n");
    }
#endif

    jlos_udp_provider_init(&g_network_stack->udp, &g_network_stack->ipv4);
#if KERNEL_CONFIG_DEBUG_NETWORK
    bool udp_ok = true;
    if (g_network_stack->udp.base_handler.backend == NULL) {
        printk_err("[FAIL] UDP backend is NULL\n");
        udp_ok = false;
    }
    if (g_network_stack->udp.base_handler.ip_protocol != 0x11) {
        printk_err("[FAIL] UDP protocol mismatch, expected=0x11 actual=0x%x\n",
            g_network_stack->udp.base_handler.ip_protocol);
        udp_ok = false;
    }
    if (g_network_stack->udp.base_handler.on_internet_protocol_received == NULL) {
        printk_err("[FAIL] UDP protocol handler is NULL\n");
        udp_ok = false;
    }
    if (g_network_stack->udp.sockets.buckets == NULL) {
        printk_err("[FAIL] UDP sockets array allocation failed\n");
        udp_ok = false;
    }
    if (g_network_stack->udp.num_sockets != 0) {
        printk_err("[FAIL] UDP num_sockets should be 0, actual=%x\n", g_network_stack->udp.num_sockets);
        udp_ok = false;
    }
    if (g_network_stack->udp.free_port != 1024) {
        printk_err("[FAIL] UDP free_port should be 1024, actual=%x\n", g_network_stack->udp.free_port);
        udp_ok = false;
    }
    if (g_network_stack->ipv4.handlers[0x11] != &g_network_stack->udp.base_handler) {
        printk_err("[FAIL] UDP not registered in IPv4 handlers\n");
        udp_ok = false;
    }
    if (udp_ok) {
        printk_info("[OK] UDP initialized (proto=0x11, sockets=%p)\n", g_network_stack->udp.sockets.buckets);
    }
#endif

    printk_info("initializing TCP protocol\n");
    jlos_tcp_provider_init(&g_network_stack->tcp, &g_network_stack->ipv4);
#if KERNEL_CONFIG_DEBUG_NETWORK
    bool tcp_ok = true;
    if (g_network_stack->tcp.base_handler.backend == NULL) {
        printk_err("[FAIL] TCP backend is NULL\n");
        tcp_ok = false;
    }
    if (g_network_stack->tcp.base_handler.ip_protocol != 0x06) {
        printk_err("[FAIL] TCP protocol mismatch, expected=0x06 actual=0x%x\n",
            g_network_stack->tcp.base_handler.ip_protocol);
        tcp_ok = false;
    }
    if (g_network_stack->tcp.base_handler.on_internet_protocol_received == NULL) {
        printk_err("[FAIL] TCP protocol handler is NULL\n");
        tcp_ok = false;
    }
    if (g_network_stack->tcp.sockets.buckets == NULL) {
        printk_err("[FAIL] TCP sockets array allocation failed\n");
        tcp_ok = false;
    }
    if (g_network_stack->tcp.num_sockets != 0) {
        printk_err("[FAIL] TCP num_sockets should be 0, actual=%x\n", g_network_stack->tcp.num_sockets);
        tcp_ok = false;
    }
    if (g_network_stack->tcp.free_port != 1024) {
        printk_err("[FAIL] TCP free_port should be 1024, actual=%x\n", g_network_stack->tcp.free_port);
        tcp_ok = false;
    }
    if (g_network_stack->ipv4.handlers[0x06] != &g_network_stack->tcp.base_handler) {
        printk_err("[FAIL] TCP not registered in IPv4 handlers\n");
        tcp_ok = false;
    }
    if (tcp_ok) {
        printk_info("[OK] TCP initialized (proto=0x06, sockets=%p)\n", g_network_stack->tcp.sockets.buckets);
    }
#endif

    printk_info("sending ARP broadcast to resolve gateway\n");
    jlos_arp_broadcast_mac_address(&g_network_stack->arp, gateway_ip_be);
    printk_info("network stack initialization complete\n");
}
