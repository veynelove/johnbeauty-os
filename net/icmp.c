#include <net/icmp.h>

#define JLOS_KERNEL_LOG_SUBSYS "icmp"
#include <kernel/printk.h>

static bool jlos_icmp_on_internet_protocol_received(jlos_icmp_t* self, jlos_net_sk_buff_t *skb)
{
    (void)self;
    printk_debug("received packet from %x.%x.%x.%x\n", skb->src_ip & 0xFF,
        (skb->src_ip >> 8) & 0xFF,
        (skb->src_ip >> 16) & 0xFF,
        (skb->src_ip >> 24) & 0xFF);
    uint32_t size = jlos_net_skb_len(skb);
    if (size < sizeof(jlos_icmp_message_t)) {
        printk_debug("packet too small\n");
        return false;
    }
    jlos_icmp_message_t *msg = (jlos_icmp_message_t *)skb->data;
    switch (msg->type) {
        case 0:
            printk_debug("echo reply received\n");
            break;
        case 8:
            printk_debug("echo request received, sending reply\n");
            msg->type = 0;
            msg->check_sum = 0;
            msg->check_sum =
                jlos_internet_protocol_provider_csum_fold(jlos_internet_protocol_provider_csum_partial(msg, size, 0));
            return true;
    }
    return false;
}

void jlos_icmp_init(jlos_icmp_t* self, jlos_internet_protocol_provider_t *backend)
{
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x01);
    self->base_handler.on_internet_protocol_received = (bool (*)(jlos_internet_protocol_handler_t *, jlos_net_sk_buff_t *))jlos_icmp_on_internet_protocol_received;
}

void jlos_icmp_destroy(jlos_icmp_t* self)
{
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

void jlos_icmp_request_echo_reply(jlos_icmp_t* self, uint32_t ip_be)
{
    uint32_t max_hdr = sizeof(jlos_ether_frame_header_t) + sizeof(jlos_ipv4_message_t);
    jlos_net_sk_buff_t *skb = jlos_net_skb_alloc(sizeof(jlos_icmp_message_t) + max_hdr);
    if (!skb) {
        return;
    }
    jlos_net_skb_reserve(skb, max_hdr);
    jlos_icmp_message_t *icmp = (jlos_icmp_message_t *)jlos_net_skb_put(skb, sizeof(jlos_icmp_message_t));
    icmp->type = 8;
    icmp->code = 0;
    icmp->data = 0x3713;
    icmp->check_sum = 0;
    icmp->check_sum = jlos_internet_protocol_provider_csum_fold(jlos_internet_protocol_provider_csum_partial(icmp, sizeof(jlos_icmp_message_t), 0));
    jlos_internet_protocol_handler_send(&self->base_handler, ip_be, skb);
}
