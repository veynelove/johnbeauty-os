#include <net/ipv4.h>
#include <kernel/memory_manager.h>
#include <tools/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "ipv4"
#include <kernel/printk.h>

void jlos_internet_protocol_handler_init(jlos_internet_protocol_handler_t* self, jlos_internet_protocol_provider_t *backend, uint8_t protocol)
{
    self->backend = backend;
    self->ip_protocol = protocol;
    self->on_internet_protocol_received = jlos_internet_protocol_handler_on_internet_protocol_received;
    backend->handlers[protocol] = self;
}

void jlos_internet_protocol_handler_destroy(jlos_internet_protocol_handler_t* self)
{
    if (self->backend->handlers[self->ip_protocol] == self) {
        self->backend->handlers[self->ip_protocol] = NULL;
    }
}

bool jlos_internet_protocol_handler_on_internet_protocol_received(jlos_internet_protocol_handler_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size)
{
    (void)self;
    (void)srcIP_BE;
    (void)dstIP_BE;
    (void)internet_protocol_payload;
    (void)size;
    return false;
}

void jlos_internet_protocol_handler_send(jlos_internet_protocol_handler_t* self, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size)
{
    jlos_internet_protocol_provider_send(self->backend, dstIP_BE, self->ip_protocol, internet_protocol_payload, size);
}

void jlos_internet_protocol_provider_init(jlos_internet_protocol_provider_t* self, jlos_ether_frame_provider_t *backend, jlos_arp_t *arp, uint32_t gateway_ip, uint32_t subnet_mask)
{
    jlos_ether_frame_handler_init(&self->base_handler, backend, 0x800);
    self->base_handler.on_ether_frame_received = (bool (*)(jlos_ether_frame_handler_t*, uint8_t*, uint32_t))jlos_internet_protocol_provider_on_ether_frame_received;
    self->arp = arp;
    self->gateway_ip = gateway_ip;
    self->subnet_mask = subnet_mask;
    for (int i = 0; i < 255; i++) {
        self->handlers[i] = NULL;
    }
}

void jlos_internet_protocol_provider_destroy(jlos_internet_protocol_provider_t* self)
{
    jlos_ether_frame_handler_destroy(&self->base_handler);
}

bool jlos_internet_protocol_provider_on_ether_frame_received(jlos_internet_protocol_provider_t* self, uint8_t *etherframe_payload, uint32_t size)
{
    printk_debug("received IPv4 packet, size=%x%x\n", size & 0xFF, (size >> 8) & 0xFF);
    if (size < sizeof(jlos_ipv4_message_t)) {
        printk_debug("packet too small\n");
        return false;
    }
    jlos_ipv4_message_t *ip_message = (jlos_ipv4_message_t *)etherframe_payload;
    bool send_back = false;
    uint8_t header_length = JLOS_IPV4_GET_IHL(ip_message);
    printk_debug("protocol=%x\n", ip_message->protocol);
    if (ip_message->dst_ip == jlos_ether_frame_provider_get_ip_address(self->base_handler.backend)) {
        printk_debug("packet is for us\n");
        int length = JLOS_SWAP_ENDIAN_16(ip_message->total_length);
        if (length > (int)size) {
            length = size;
        }
        if (self->handlers[ip_message->protocol]) {
            printk_debug("handler found, calling it\n");
            send_back = self->handlers[ip_message->protocol]->on_internet_protocol_received(
                self->handlers[ip_message->protocol], ip_message->src_ip, ip_message->dst_ip,
                etherframe_payload + 4 * header_length, length - 4 * header_length);
        }
        else {
            printk_debug("no handler for this protocol\n");
        }
    }
    else {
        printk_debug("packet not for us\n");
    }
    if (send_back) {
        uint32_t temp = ip_message->dst_ip;
        ip_message->dst_ip = ip_message->src_ip;
        ip_message->src_ip = temp;
        ip_message->time_to_live = 0x40;
        ip_message->checksum = 0;
        ip_message->checksum = jlos_internet_protocol_provider_check_sum((uint16_t *)ip_message, 4 * header_length);
    }
    return send_back;
}

void jlos_internet_protocol_provider_send(jlos_internet_protocol_provider_t* self, uint32_t dstIP_BE, uint8_t protocol, uint8_t *data, uint32_t size)
{
    uint8_t *buffer = (uint8_t *)jlos_kalloc(sizeof(jlos_ipv4_message_t) + size);
    jlos_ipv4_message_t *message = (jlos_ipv4_message_t *)buffer;
    uint8_t ihl = sizeof(jlos_ipv4_message_t) / 4;
    JLOS_IPV4_SET_VERSION_IHL(message, 4, ihl);
    message->tos = 0;
    message->total_length = JLOS_SWAP_ENDIAN_16(size + sizeof(jlos_ipv4_message_t));
    message->ident = 0x0100;
    message->flags_and_offset = 0x0040;
    message->time_to_live = 0x40;
    message->protocol = protocol;
    message->dst_ip = dstIP_BE;
    message->src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    message->checksum = 0;
    message->checksum = jlos_internet_protocol_provider_check_sum((uint16_t *)message, sizeof(jlos_ipv4_message_t));
    uint8_t *data_buffer = buffer + sizeof(jlos_ipv4_message_t);
    for (int i = 0; i < (int)size; i++) {
        data_buffer[i] = data[i];
    }
    uint32_t route = dstIP_BE;
    if ((dstIP_BE & self->subnet_mask) != (message->src_ip & self->subnet_mask)) {
        route = self->gateway_ip;
    }
    uint64_t dst_mac = jlos_arp_lookup_or_request(self->arp, route);
    if (dst_mac == 0xFFFFFFFFFFFF) {
        printk_warn("ARP pending for route=%x, packet dropped\n", route);
        jlos_kfree(buffer);
        return;
    }
    jlos_ether_frame_handler_send(&self->base_handler, dst_mac, self->base_handler.etherType_BE, buffer, sizeof(jlos_ipv4_message_t) + size);
    jlos_kfree(buffer);
}

uint16_t jlos_internet_protocol_provider_check_sum(uint16_t *data, uint32_t length_in_bytes)
{
    uint32_t temp = 0;
    for (int i = 0; i < (int)(length_in_bytes / 2); i++) {
        temp += JLOS_SWAP_ENDIAN_16(data[i]);
    }
    if (length_in_bytes % 2) {
        temp += (uint16_t)(((char *)data)[length_in_bytes - 1]) << 8;
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
