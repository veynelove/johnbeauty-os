#include <net/ipv4.h>
#include <kernel/memory_manager.h>
#include <include/config.h>

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

bool jlos_internet_protocol_handler_on_internet_protocol_received(jlos_internet_protocol_handler_t* self, jlos_net_sk_buff_t *skb)
{
    (void)self;
    (void)skb;
    return false;
}

void jlos_internet_protocol_handler_send(jlos_internet_protocol_handler_t* self, uint32_t dstIP_BE, jlos_net_sk_buff_t *skb)
{
    jlos_internet_protocol_provider_send(self->backend, dstIP_BE, self->ip_protocol, skb);
}

static bool jlos_internet_protocol_provider_on_ether_frame_received(jlos_internet_protocol_provider_t* self, jlos_net_sk_buff_t *skb)
{
    uint32_t size = jlos_net_skb_len(skb);
    if (size < sizeof(jlos_ipv4_message_t)) {
        printk_debug("packet too small\n");
        return false;
    }
    jlos_ipv4_message_t *ip_message = (jlos_ipv4_message_t *)skb->data;
    bool send_back = false;
    uint8_t header_length = JLOS_IPV4_GET_IHL(ip_message);
    if (ip_message->dst_ip == jlos_ether_frame_provider_get_ip_address(self->base_handler.backend) || ip_message->dst_ip == JLOS_IPV4_BROADCAST) {
        printk_debug("packet is for us\n");
        int length = JLOS_SWAP_ENDIAN_16(ip_message->total_length);
        if (length > (int)size) {
            length = size;
        }
        if (self->handlers[ip_message->protocol]) {
            printk_debug("handler found, calling it\n");
            skb->src_ip = ip_message->src_ip;
            skb->dst_ip = ip_message->dst_ip;
            skb->protocol = ip_message->protocol;
            jlos_net_skb_pull(skb, 4 *header_length);
            send_back = self->handlers[ip_message->protocol]->on_internet_protocol_received(self->handlers[ip_message->protocol], skb);
            if (send_back) {
                jlos_net_skb_push(skb, 4 * header_length);
            }
        }
        else {
            printk_debug("no handler for this protocol\n");
        }
    }
    if (send_back) {
        uint32_t temp = ip_message->dst_ip;
        ip_message->dst_ip = ip_message->src_ip;
        ip_message->src_ip = temp;
        ip_message->time_to_live = 0x40;
        ip_message->checksum = 0;
        ip_message->checksum =
            jlos_internet_protocol_provider_csum_fold(jlos_internet_protocol_provider_csum_partial(ip_message, 4 * header_length, 0));
    }
    return send_back;
}

void jlos_internet_protocol_provider_init(jlos_internet_protocol_provider_t* self, jlos_ether_frame_provider_t *backend, jlos_arp_t *arp, uint32_t gateway_ip, uint32_t subnet_mask)
{
    jlos_ether_frame_handler_init(&self->base_handler, backend, 0x800);
    self->base_handler.on_ether_frame_received = (bool (*)(jlos_ether_frame_handler_t *, jlos_net_sk_buff_t *))jlos_internet_protocol_provider_on_ether_frame_received;
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

void jlos_internet_protocol_provider_send(jlos_internet_protocol_provider_t* self, uint32_t dstIP_BE, uint8_t protocol, jlos_net_sk_buff_t *skb)
{

    jlos_ipv4_message_t *message = (jlos_ipv4_message_t *)jlos_net_skb_push(skb, sizeof(jlos_ipv4_message_t));
    uint8_t ihl = sizeof(jlos_ipv4_message_t) / 4;
    JLOS_IPV4_SET_VERSION_IHL(message, 4, ihl);
    message->tos = 0;
    message->total_length = JLOS_SWAP_ENDIAN_16(jlos_net_skb_len(skb));
    message->ident = 0x0100;
    message->flags_and_offset = 0x0040;
    message->time_to_live = 0x40;
    message->protocol = protocol;
    message->dst_ip = dstIP_BE;
    message->src_ip = jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
    message->checksum = 0;
    message->checksum =
        jlos_internet_protocol_provider_csum_fold(jlos_internet_protocol_provider_csum_partial(message, sizeof(jlos_ipv4_message_t), 0));
    uint64_t dst_mac;
    if (dstIP_BE == JLOS_IPV4_BROADCAST) {
        dst_mac = JLOS_ETHER_BROADCAST_MAC;
    } else {
        uint32_t route = dstIP_BE;
        if ((dstIP_BE & self->subnet_mask) != (message->src_ip & self->subnet_mask)) {
            route = self->gateway_ip;
        }
        dst_mac = jlos_arp_lookup_or_request(self->arp, route);
        if (dst_mac == 0xFFFFFFFFFFFF) {
            printk_warn("ARP pending for route=%x, packet dropped\n", route);
            jlos_net_skb_free(skb);
            return;
        }
    }
    
    jlos_ether_frame_handler_send(&self->base_handler, dst_mac, self->base_handler.etherType_BE, skb);
}

uint32_t jlos_internet_protocol_provider_csum_partial(const void *data, uint32_t length_in_bytes, uint32_t init_csum)
{
    uint32_t csum = init_csum;
    const uint16_t *p = (const uint16_t *)data;
    uint32_t n = length_in_bytes >> 1;
    for (uint32_t i = 0; i < n; i++) {
        csum += JLOS_SWAP_ENDIAN_16(p[i]);
    }
    if (length_in_bytes & 1) {
        csum += (uint16_t)(((const uint8_t *)data)[length_in_bytes - 1]) << 8;
    }
    return csum;
}

uint16_t jlos_internet_protocol_provider_csum_fold(uint32_t csum)
{
    while (csum >> 16) {
        csum = (csum & 0xFFFF) + (csum >> 16);
    }
    return JLOS_SWAP_ENDIAN_16(~csum);
}

uint32_t jlos_internet_protocol_provider_get_ip_address(jlos_internet_protocol_provider_t* self)
{
    return jlos_ether_frame_provider_get_ip_address(self->base_handler.backend);
}
