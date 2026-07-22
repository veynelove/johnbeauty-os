#include <net/ipv4.h>
#include <kernel/memory_manager.h>
#include <tools/config.h>

extern void printf(const char *str);
extern void printf_hex(uint8_t);
extern void printf_hex32(uint32_t);

void jlos_internet_protocol_handler_init(jlos_internet_protocol_handler_t* self, jlos_internet_protocol_provider_t *backend, uint8_t m_protocol)
{
    self->backend = backend;
    self->m_ip_protocol = m_protocol;
    self->on_internet_protocol_received = jlos_internet_protocol_handler_on_internet_protocol_received;
    backend->handlers[m_protocol] = self;
}

void jlos_internet_protocol_handler_destroy(jlos_internet_protocol_handler_t* self)
{
    if (self->backend->handlers[self->m_ip_protocol] == self) {
        self->backend->handlers[self->m_ip_protocol] = NULL;
    }
}

bool jlos_internet_protocol_handler_on_internet_protocol_received(jlos_internet_protocol_handler_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size)
{
    return false;
}

void jlos_internet_protocol_handler_send(jlos_internet_protocol_handler_t* self, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size)
{
    jlos_internet_protocol_provider_send(self->backend, dstIP_BE, self->m_ip_protocol, internet_protocol_payload, m_size);
}

void jlos_internet_protocol_provider_init(jlos_internet_protocol_provider_t* self, jlos_ether_frame_provider_t *backend, jlos_arp_t *arp, uint32_t m_gateway_ip, uint32_t m_subnet_mask)
{
    jlos_ether_frame_handler_init(&self->base_handler, backend, 0x800);
    self->base_handler.on_ether_frame_received = (bool (*)(jlos_ether_frame_handler_t*, uint8_t*, uint32_t))jlos_internet_protocol_provider_on_ether_frame_received;
    self->arp = arp;
    self->m_gateway_ip = m_gateway_ip;
    self->m_subnet_mask = m_subnet_mask;
    
    for (int i = 0; i < 255; i++) {
        self->handlers[i] = NULL;
    }
}

void jlos_internet_protocol_provider_destroy(jlos_internet_protocol_provider_t* self)
{
    jlos_ether_frame_handler_destroy(&self->base_handler);
}

bool jlos_internet_protocol_provider_on_ether_frame_received(jlos_internet_protocol_provider_t* self, uint8_t *etherframe_payload, uint32_t m_size)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("IP: Received IPv4 packet, size=");
    printf_hex((m_size >> 0) & 0xFF);
    printf_hex((m_size >> 8) & 0xFF);
    printf("\n");
#endif
    
    if (m_size < sizeof(jlos_ipv4_message_t)) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("IP: Packet too small\n");
#endif
        return false;
    }
    jlos_ipv4_message_t *ip_message = (jlos_ipv4_message_t *)etherframe_payload;
    bool send_back = false;
    
    uint8_t header_length = JLOS_IPV4_GET_IHL(ip_message);
    
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("IP: Protocol=");
    printf_hex(ip_message->m_protocol);
    printf("\n");
#endif
    
    if (ip_message->m_dst_ip == jlos_ether_frame_provider_get_ip_address(self->base_handler.backend)) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("IP: Packet is for us\n");
#endif
        int m_length = JLOS_SWAP_ENDIAN_16(ip_message->m_total_length);
        if (m_length > (int)m_size) {
            m_length = m_size;
        }
        if (self->handlers[ip_message->m_protocol]) {
#if KERNEL_CONFIG_DEBUG_NETWORK
            printf("IP: Handler found, calling it\n");
#endif
            send_back = self->handlers[ip_message->m_protocol]->on_internet_protocol_received(
                self->handlers[ip_message->m_protocol], ip_message->m_src_ip, ip_message->m_dst_ip,
                etherframe_payload + 4 * header_length, m_length - 4 * header_length);
        }
#if KERNEL_CONFIG_DEBUG_NETWORK
        else {
            printf("IP: No handler for this protocol\n");
        }
#endif
    }
#if KERNEL_CONFIG_DEBUG_NETWORK
    else {
        printf("IP: Packet not for us\n");
    }
#endif
    
    if (send_back) {
        uint32_t temp = ip_message->m_dst_ip;
        ip_message->m_dst_ip = ip_message->m_src_ip;
        ip_message->m_src_ip = temp;

        ip_message->m_time_to_live = 0x40;
        ip_message->m_checksum = 0;
        ip_message->m_checksum = jlos_internet_protocol_provider_check_sum((uint16_t *)ip_message, 4 * header_length);
    }
    return send_back;
}

void jlos_internet_protocol_provider_send(jlos_internet_protocol_provider_t* self, uint32_t dstIP_BE, uint8_t m_protocol, uint8_t *m_data, uint32_t m_size)
{
    uint8_t *buffer = (uint8_t *)jlos_malloc(sizeof(jlos_ipv4_message_t) + m_size);
    jlos_ipv4_message_t *message = (jlos_ipv4_message_t *)buffer;
    uint8_t ihl = sizeof(jlos_ipv4_message_t) / 4;
    JLOS_IPV4_SET_VERSION_IHL(message, 4, ihl);
    message->m_tos = 0;
    message->m_total_length = JLOS_SWAP_ENDIAN_16(m_size + sizeof(jlos_ipv4_message_t));
    message->m_ident = 0x0100;
    message->flags_and_offset = 0x0040;
    message->m_time_to_live = 0x40;
    message->m_protocol = m_protocol;
    
    message->m_dst_ip = dstIP_BE;
    message->m_src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);

    message->m_checksum = 0;
    message->m_checksum = jlos_internet_protocol_provider_check_sum((uint16_t *)message, sizeof(jlos_ipv4_message_t));

    uint8_t *data_buffer = buffer + sizeof(jlos_ipv4_message_t);
    for (int i = 0; i < (int)m_size; i++) {
        data_buffer[i] = m_data[i];
    }
    uint32_t route = dstIP_BE;
    if ((dstIP_BE & self->m_subnet_mask) != (message->m_src_ip & self->m_subnet_mask)) {
        route = self->m_gateway_ip;
    }
    uint64_t dst_mac = jlos_arp_lookup_or_request(self->arp, route);
    if (dst_mac == 0xFFFFFFFFFFFF) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("IP4: ARP pending for route=");
        printf_hex32(route);
        printf(", packet dropped\n");
#endif
        jlos_free(buffer);
        return;
    }
    jlos_ether_frame_handler_send(&self->base_handler, dst_mac, self->base_handler.m_etherType_BE, buffer, sizeof(jlos_ipv4_message_t) + m_size);
    jlos_free(buffer);
}

uint16_t jlos_internet_protocol_provider_check_sum(uint16_t *m_data, uint32_t length_in_bytes)
{
    uint32_t temp = 0;
    for (int i = 0; i < (int)(length_in_bytes / 2); i++) {
        temp += JLOS_SWAP_ENDIAN_16(m_data[i]);
    }
    if (length_in_bytes % 2) {
        temp += (uint16_t)(((char *)m_data)[length_in_bytes - 1]) << 8;
    }
    while (temp & 0xFFFF0000) {
        temp = (temp & 0xFFFF) + (temp >> 16);
    }
    return JLOS_SWAP_ENDIAN_16(~temp);
}

uint32_t jlos_internet_protocol_provider_get_ip_address(jlos_internet_protocol_provider_t* self)
{
    return jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
}