#include <net/arp.h>
#include <tools/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "arp"
#include <kernel/printk.h>

static void uint64_to_mac(uint64_t mac_be, uint8_t *dest)
{
    for (int i = 0; i < 6; i++) {
        dest[i] = (mac_be >> (8 * i)) & 0xFF;
    }
}

static uint64_t mac_to_uint64(uint8_t *mac)
{
    uint64_t result = 0;
    for (int i = 0; i < 6; i++) {
        result |= ((uint64_t)mac[i]) << (8 * i);
    }
    return result;
}

void jlos_arp_init(jlos_arp_t* self, jlos_ether_frame_provider_t *backend)
{
    printk_info("initializing ARP protocol\n");
    self->num_cache_entries = 0;
    jlos_ether_frame_handler_init(&self->base_handler, backend, 0x806);
    self->base_handler.on_ether_frame_received = (bool (*)(jlos_ether_frame_handler_t*, uint8_t*, uint32_t))jlos_arp_on_ether_frame_received;
}

void jlos_arp_destroy(jlos_arp_t* self)
{
    jlos_ether_frame_handler_destroy(&self->base_handler);
}

bool jlos_arp_on_ether_frame_received(jlos_arp_t* self, uint8_t *etherframe_payload, uint32_t size)
{
    if (size < sizeof(jlos_arp_message_t)) {
        printk_debug("packet too small\n");
        return false;
    }
    jlos_arp_message_t *arp = (jlos_arp_message_t *)etherframe_payload;
    if (arp->hardware_type == 0x0100) {
        if (arp->protocol == 0x0008 && arp->hardware_address_size == 6
            && arp->protocol_address_size == 4 && arp->dst_ip == jlos_ether_frame_provider_get_ip_address(self->base_handler.backend)) {
            switch (arp->command) {
                case 0x0100: // ARP Request
                    printk_debug("received ARP request\n");
                    arp->command = 0x0200;
                    arp->dst_ip = arp->src_ip;
                    for (int i = 0; i < 6; i++) {
                        arp->dst_mac[i] = arp->src_mac[i];
                    }
                    arp->src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
                    uint64_to_mac(jlos_ether_frame_provider_get_mac_address(self->base_handler.backend), arp->src_mac);
                    printk_debug("sending ARP reply\n");
                    return true;
                case 0x0200: // ARP Reply
                    printk_debug("received ARP reply\n");
                    if (self->num_cache_entries < 128) {
                        self->ip_cache[self->num_cache_entries] = arp->src_ip;
                        self->mac_cache[self->num_cache_entries] = mac_to_uint64(arp->src_mac);
                        self->num_cache_entries++;
                        printk_debug("cache updated\n");
                    }
                    break;
            }
        }
    }
    return false;
}

void jlos_arp_broadcast_mac_address(jlos_arp_t* self, uint32_t IP_BE)
{
    jlos_arp_message_t arp;
    arp.hardware_type = 0x0100;
    arp.protocol = 0x0008;
    arp.hardware_address_size = 6;
    arp.protocol_address_size = 4;
    arp.command = 0x0200;
    uint64_to_mac(jlos_ether_frame_provider_get_mac_address(self->base_handler.backend), arp.src_mac);
    arp.src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    uint64_t dst_mac_be = jlos_arp_resolve(self, IP_BE);
    uint64_to_mac(dst_mac_be, arp.dst_mac);
    arp.dst_ip = IP_BE;
    jlos_ether_frame_handler_send(&self->base_handler, dst_mac_be, self->base_handler.etherType_BE, (uint8_t *)&arp, sizeof(jlos_arp_message_t));
    printk_debug("broadcast complete\n");
}

void jlos_arp_request_mac_address(jlos_arp_t* self, uint32_t IP_BE)
{
    jlos_arp_message_t arp;
    arp.hardware_type = 0x0100;
    arp.protocol = 0x0008;
    arp.hardware_address_size = 6;
    arp.protocol_address_size = 4;
    arp.command = 0x0100;
    uint64_to_mac(jlos_ether_frame_provider_get_mac_address(self->base_handler.backend), arp.src_mac);
    arp.src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    for (int i = 0; i < 6; i++) {
        arp.dst_mac[i] = 0xFF;
    }
    arp.dst_ip = IP_BE;
    jlos_ether_frame_handler_send(&self->base_handler, 0xFFFFFFFFFFFF, self->base_handler.etherType_BE, (uint8_t *)&arp, sizeof(jlos_arp_message_t));
    printk_debug("request sent\n");
}

uint64_t jlos_arp_get_mac_from_cache(jlos_arp_t* self, uint32_t IP_BE)
{
    for (int i = 0; i < self->num_cache_entries; i++) {
        if (self->ip_cache[i] == IP_BE) {
            return self->mac_cache[i];
        }
    }
    return 0xFFFFFFFFFFFF;
}

uint64_t jlos_arp_lookup_or_request(jlos_arp_t* self, uint32_t IP_BE)
{
    uint64_t result = jlos_arp_get_mac_from_cache(self, IP_BE);
    if (result == 0xFFFFFFFFFFFF) {
        printk_debug("lookup miss, sending request for %x\n", IP_BE);
        jlos_arp_request_mac_address(self, IP_BE);
    }
    return result;
}

uint64_t jlos_arp_resolve(jlos_arp_t* self, uint32_t IP_BE)
{
    uint64_t result = jlos_arp_get_mac_from_cache(self, IP_BE);
    if (result == 0xFFFFFFFFFFFF) {
        printk_debug("sending request for %x\n", IP_BE);
        jlos_arp_request_mac_address(self, IP_BE);
        result = jlos_arp_get_mac_from_cache(self, IP_BE);
    }
    /* 忙等 + CPU relax，避免 hlt 卡死；超时返回 FFFF 标记未解析 */
    volatile uint32_t timeout = 0;
    while (result == 0xFFFFFFFFFFFF && timeout < 5000000) {
        __asm__ __volatile__("pause" ::: "memory");
        timeout++;
        result = jlos_arp_get_mac_from_cache(self, IP_BE);
    }
    if (result != 0xFFFFFFFFFFFF) {
        printk_debug("resolved MAC=0x%X%X\n", (uint32_t)(result >> 32), (uint32_t)(result & 0xFFFFFFFF));
    } else {
        printk_warn("resolve timeout for %x\n", IP_BE);
    }
    return result;
}
