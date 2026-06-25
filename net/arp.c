#include <net/arp.h>

void jlos_arp_init(jlos_arp_t* self, jlos_ether_frame_provider_t *backend)
{
    jlos_ether_frame_handler_init(&self->base_handler, backend, 0x806);
    self->base_handler.on_ether_frame_received = (bool (*)(jlos_ether_frame_handler_t*, uint8_t*, uint32_t))jlos_arp_on_ether_frame_received;
    self->m_num_cache_entries = 0;
}

void jlos_arp_destroy(jlos_arp_t* self)
{
    jlos_ether_frame_handler_destroy(&self->base_handler);
}

bool jlos_arp_on_ether_frame_received(jlos_arp_t* self, uint8_t *etherframe_payload, uint32_t m_size)
{
    if (m_size < sizeof(jlos_arp_message_t)) {
        return false;
    }
    jlos_arp_message_t *arp = (jlos_arp_message_t *)etherframe_payload;
    if (arp->m_hardware_type == 0x0100) {
        if (arp->m_protocol == 0x0008 && arp->m_hardware_address_size == 6
            && arp->m_protocol_address_size == 4 && arp->m_dst_ip == jlos_ether_frame_provider_get_ip_address(self->base_handler.backend)) {
            switch (arp->m_command) {
                case 0x0100:
                    arp->m_command = 0x0200;
                    arp->m_dst_ip = arp->m_src_ip;
                    arp->dst_mac = arp->src_mac;
                    arp->m_src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
                    arp->src_mac = jlos_ether_frame_provider_get_mac_address(self->base_handler.backend);
                    return true;
                case 0x0200:
                    if (self->m_num_cache_entries < 128) {
                        self->ip_cache[self->m_num_cache_entries] = arp->m_src_ip;
                        self->mac_cache[self->m_num_cache_entries] = arp->src_mac;
                        self->m_num_cache_entries++;
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
    arp.m_hardware_type = 0x0100;
    arp.m_protocol = 0x0008;
    arp.m_hardware_address_size = 6;
    arp.m_protocol_address_size = 4;
    arp.m_command = 0x0200;

    arp.src_mac = jlos_ether_frame_provider_get_mac_address(self->base_handler.backend);
    arp.m_src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    arp.dst_mac = jlos_arp_resolve(self, IP_BE);
    arp.m_dst_ip = IP_BE;
    jlos_ether_frame_handler_send(&self->base_handler, arp.dst_mac, self->base_handler.m_etherType_BE, (uint8_t *)&arp, sizeof(jlos_arp_message_t));
}

void jlos_arp_request_mac_address(jlos_arp_t* self, uint32_t IP_BE)
{
    jlos_arp_message_t arp;
    arp.m_hardware_type = 0x0100;
    arp.m_protocol = 0x0008;
    arp.m_hardware_address_size = 6;
    arp.m_protocol_address_size = 4;
    arp.m_command = 0x0100;

    arp.src_mac = jlos_ether_frame_provider_get_mac_address(self->base_handler.backend);
    arp.m_src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    arp.dst_mac = 0xFFFFFFFFFFFF;
    arp.m_dst_ip = IP_BE;
    jlos_ether_frame_handler_send(&self->base_handler, arp.dst_mac, self->base_handler.m_etherType_BE, (uint8_t *)&arp, sizeof(jlos_arp_message_t));
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

uint64_t jlos_arp_resolve(jlos_arp_t* self, uint32_t IP_BE)
{
    uint64_t result = jlos_arp_get_mac_from_cache(self, IP_BE);
    if (result == 0xFFFFFFFFFFFF) {
        jlos_arp_request_mac_address(self, IP_BE);
    }
    while (result == 0xFFFFFFFFFFFF) {
        __asm__ __volatile__("hlt");
        result = jlos_arp_get_mac_from_cache(self, IP_BE);
    }
    return result;
}