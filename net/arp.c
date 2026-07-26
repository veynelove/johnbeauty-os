#include <net/arp.h>
#include <tools/config.h>

extern void printf(const char *str);
extern void printf_hex32(uint32_t);

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
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("ARP: Initializing ARP protocol...\n");
#endif
    self->m_num_cache_entries = 0;
    jlos_ether_frame_handler_init(&self->base_handler, backend, 0x806);
    self->base_handler.on_ether_frame_received = (bool (*)(jlos_ether_frame_handler_t*, uint8_t*, uint32_t))jlos_arp_on_ether_frame_received;
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("ARP: ARP protocol initialized successfully\n");
#endif
}

void jlos_arp_destroy(jlos_arp_t* self)
{
    jlos_ether_frame_handler_destroy(&self->base_handler);
}

bool jlos_arp_on_ether_frame_received(jlos_arp_t* self, uint8_t *etherframe_payload, uint32_t m_size)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("ARP: Received ARP packet\n");
#endif
    
    if (m_size < sizeof(jlos_arp_message_t)) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("ARP: Packet too small\n");
#endif
        return false;
    }
    jlos_arp_message_t *arp = (jlos_arp_message_t *)etherframe_payload;
    if (arp->m_hardware_type == 0x0100) {
        if (arp->m_protocol == 0x0008 && arp->m_hardware_address_size == 6
            && arp->m_protocol_address_size == 4 && arp->m_dst_ip == jlos_ether_frame_provider_get_ip_address(self->base_handler.backend)) {
            switch (arp->m_command) {
                case 0x0100: // ARP Request
#if KERNEL_CONFIG_DEBUG_NETWORK
                    printf("ARP: Received ARP REQUEST\n");
#endif
                    arp->m_command = 0x0200;
                    arp->m_dst_ip = arp->m_src_ip;
                    for (int i = 0; i < 6; i++) {
                        arp->dst_mac[i] = arp->src_mac[i];
                    }
                    arp->m_src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
                    uint64_to_mac(jlos_ether_frame_provider_get_mac_address(self->base_handler.backend), arp->src_mac);
#if KERNEL_CONFIG_DEBUG_NETWORK
                    printf("ARP: Sending ARP REPLY\n");
#endif
                    return true;
                case 0x0200: // ARP Reply
#if KERNEL_CONFIG_DEBUG_NETWORK
                    printf("ARP: Received ARP REPLY\n");
#endif
                    if (self->m_num_cache_entries < 128) {
                        self->ip_cache[self->m_num_cache_entries] = arp->m_src_ip;
                        self->mac_cache[self->m_num_cache_entries] = mac_to_uint64(arp->src_mac);
                        self->m_num_cache_entries++;
#if KERNEL_CONFIG_DEBUG_NETWORK
                        printf("ARP: Cache updated\n");
#endif
                    }
                    break;
            }
        }
    }
    return false;
}

void jlos_arp_broadcast_mac_address(jlos_arp_t* self, uint32_t IP_BE)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("ARP: Broadcasting ARP request\n");
#endif
    
    jlos_arp_message_t arp;
    arp.m_hardware_type = 0x0100;
    arp.m_protocol = 0x0008;
    arp.m_hardware_address_size = 6;
    arp.m_protocol_address_size = 4;
    arp.m_command = 0x0200;

    uint64_to_mac(jlos_ether_frame_provider_get_mac_address(self->base_handler.backend), arp.src_mac);
    arp.m_src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    uint64_t dst_mac_be = jlos_arp_resolve(self, IP_BE);
    uint64_to_mac(dst_mac_be, arp.dst_mac);
    arp.m_dst_ip = IP_BE;
    jlos_ether_frame_handler_send(&self->base_handler, dst_mac_be, self->base_handler.m_etherType_BE, (uint8_t *)&arp, sizeof(jlos_arp_message_t));
    
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("ARP: Broadcast complete\n");
#endif
}

void jlos_arp_request_mac_address(jlos_arp_t* self, uint32_t IP_BE)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("ARP: Requesting MAC address\n");
#endif
    
    jlos_arp_message_t arp;
    arp.m_hardware_type = 0x0100;
    arp.m_protocol = 0x0008;
    arp.m_hardware_address_size = 6;
    arp.m_protocol_address_size = 4;
    arp.m_command = 0x0100;

    uint64_to_mac(jlos_ether_frame_provider_get_mac_address(self->base_handler.backend), arp.src_mac);
    arp.m_src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    for (int i = 0; i < 6; i++) {
        arp.dst_mac[i] = 0xFF;
    }
    arp.m_dst_ip = IP_BE;
    jlos_ether_frame_handler_send(&self->base_handler, 0xFFFFFFFFFFFF, self->base_handler.m_etherType_BE, (uint8_t *)&arp, sizeof(jlos_arp_message_t));
    
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("ARP: Request sent\n");
#endif
}

uint64_t jlos_arp_get_mac_from_cache(jlos_arp_t* self, uint32_t IP_BE)
{
    for (int i = 0; i < self->m_num_cache_entries; i++) {
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
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("ARP: lookup miss, sending request for ");
        printf_hex32(IP_BE);
        printf("\n");
#endif
        jlos_arp_request_mac_address(self, IP_BE);
    }
    return result;
}

uint64_t jlos_arp_resolve(jlos_arp_t* self, uint32_t IP_BE)
{
    uint64_t result = jlos_arp_get_mac_from_cache(self, IP_BE);
    
    if (result == 0xFFFFFFFFFFFF) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("ARP: sending request for ");
        printf_hex32(IP_BE);
        printf("\n");
#endif
        jlos_arp_request_mac_address(self, IP_BE);
        result = jlos_arp_get_mac_from_cache(self, IP_BE);
    }
    
    volatile uint32_t timeout = 0;
    while (result == 0xFFFFFFFFFFFF && timeout < 5000000) {
        /* 不再 hlt 停机等中断，改成忙等 + CPU relax。
         * hlt 需要 IF=1 + 有中断定时唤醒，组合条件太脆弱容易卡死。
         * 忙等 5M 次大概几毫秒，超时就返回 FFFF 标记未解析。
         */
        __asm__ __volatile__("pause" ::: "memory");
        timeout++;
        result = jlos_arp_get_mac_from_cache(self, IP_BE);
    }
#if KERNEL_CONFIG_DEBUG_NETWORK
    if (result != 0xFFFFFFFFFFFF) {
        printf("ARP: resolved MAC=0x");
        printf_hex32(result >> 32);
        printf_hex32(result & 0xFFFFFFFF);
        printf("\n");
    } else {
        printf("ARP: resolve TIMEOUT for ");
        printf_hex32(IP_BE);
        printf("\n");
    }
#endif
    return result;
}