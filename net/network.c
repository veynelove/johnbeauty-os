/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <net/network.h>
#include <net/skbuff.h>
#include <hal/net.h>
#include <kernel/memory_manager.h>
#include <kernel/initcall.h>
#include <kernel/softirq.h>
#include <include/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "net"
#include <kernel/printk.h>

network_stack_t *g_network_stack = nullptr;

static void jlos_net_rx_action(void)
{
    jlos_hal_net_dev_t *dev = jlos_hal_net_primary_dev();
    if (dev && dev->ops && dev->ops->poll) {
        dev->ops->poll(dev);
    }
}

void network_init(void)
{
    g_network_stack = (network_stack_t *)jlos_kalloc(sizeof(network_stack_t));
    if (!g_network_stack) {
        printk_err("network init failed. g_network_stack is nullptr\n");
        return;
    }

    jlos_net_skb_init();
    jlos_hal_net_dev_t *eth0 = jlos_hal_net_primary_dev();
    if (eth0 == nullptr) {
        printk_warn("no network device, skipping network stack init\n");
        g_network_stack = nullptr;
        return;
    }

    jlos_softirq_register(JLOS_SOFTIRQ_NET_RX, jlos_net_rx_action);
    jlos_memset(&g_network_stack->config, 0, sizeof(g_network_stack->config));
    printk_info("initializing network stack (unconfigured)\n");
    jlos_hal_net_set_ip_address(eth0, g_network_stack->config.ip);

    jlos_ether_frame_provider_init(&g_network_stack->etherframe, eth0);

    jlos_arp_init(&g_network_stack->arp, &g_network_stack->etherframe);

    jlos_internet_protocol_provider_init(&g_network_stack->ipv4,
        &g_network_stack->etherframe, &g_network_stack->arp, g_network_stack->config.gateway, g_network_stack->config.subnet_mask);
    
    jlos_icmp_init(&g_network_stack->icmp, &g_network_stack->ipv4);
    
    jlos_udp_provider_init(&g_network_stack->udp, &g_network_stack->ipv4);
    
    jlos_tcp_provider_init(&g_network_stack->tcp, &g_network_stack->ipv4);

    uint64_t mac = jlos_ether_frame_provider_get_mac_address(&g_network_stack->etherframe);
    uint8_t mac_bytes[6];
    for (int i = 0; i < 6; i++) {
        mac_bytes[i] = (mac >> (8 * i)) & 0xFF;
    }
    jlos_dhcp_client_init(&g_network_stack->dhcp, &g_network_stack->udp, mac_bytes);
    jlos_dns_resolver_init(&g_network_stack->dns, &g_network_stack->udp);
    
    if (g_network_stack->config.gateway != 0) {
         printk_info("sending ARP broadcast to resolve gateway\n");
        jlos_arp_broadcast_mac_address(&g_network_stack->arp, g_network_stack->config.gateway);
    }
    printk_info("network stack initialization complete\n");
}

void jlos_network_apply_config(void)
{
    jlos_hal_net_set_ip_address(jlos_hal_net_primary_dev(), g_network_stack->config.ip);
    g_network_stack->ipv4.gateway_ip = g_network_stack->config.gateway;
    g_network_stack->ipv4.subnet_mask = g_network_stack->config.subnet_mask;
}

static void jlos_network_dhcp_start(void)
{
    if (!g_network_stack) {
        return;
    }
    if (g_network_stack->dhcp.state == JLOS_DHCP_STATE_FAILED) {
        return;
    }
    jlos_dhcp_client_discover(&g_network_stack->dhcp);
}

JLOS_INITCALL(JLOS_INITCALL_POST, jlos_network_dhcp_start);
JLOS_INITCALL(JLOS_INITCALL_LATE, network_init);
