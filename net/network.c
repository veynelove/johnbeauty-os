#include <net/network.h>
#include <kernel/memory_manager.h>
#include <kernel/initcall.h>
#include <tools/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "net"
#include <kernel/printk.h>

extern void jlos_amd_am79c973_activate(jlos_amd_am79c973_t*);

network_stack_t *g_network_stack = NULL;

void network_init(void)
{
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
    
    jlos_memset(&g_network_stack->config, 0, sizeof(g_network_stack->config));
    printk_info("initializing network stack (unconfigured)\n");
    jlos_amd_am79c973_set_ip_address(eth0, g_network_stack->config.ip);

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
    jlos_amd_am79c973_t *eth0 = g_network_stack->etherframe.base_handler.backend;
    jlos_amd_am79c973_set_ip_address(eth0, g_network_stack->config.ip);
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
JLOS_INITCALL(JLOS_INITCALL_DEVICE, network_init);
