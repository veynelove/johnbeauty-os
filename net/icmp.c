#include <net/icmp.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "icmp"

void jlos_icmp_init(jlos_icmp_t* self, jlos_internet_protocol_provider_t *backend)
{
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x01);
    self->base_handler.on_internet_protocol_received = (bool (*)(jlos_internet_protocol_handler_t*, uint32_t, uint32_t, uint8_t*, uint32_t))jlos_icmp_on_internet_protocol_received;
}

void jlos_icmp_destroy(jlos_icmp_t* self)
{
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

bool jlos_icmp_on_internet_protocol_received(jlos_icmp_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size)
{
    (void)self;
    (void)dstIP_BE;
    printk_debug("received packet from %x.%x.%x.%x\n",
        srcIP_BE & 0xFF,
        (srcIP_BE >> 8) & 0xFF,
        (srcIP_BE >> 16) & 0xFF,
        (srcIP_BE >> 24) & 0xFF);
    if (size < sizeof(jlos_icmp_message_t)) {
        printk_debug("packet too small\n");
        return false;
    }
    jlos_icmp_message_t *msg = (jlos_icmp_message_t *)internet_protocol_payload;
    switch (msg->type) {
        case 0:
            printk_debug("echo reply received\n");
            break;
        case 8:
            printk_debug("echo request received, sending reply\n");
            msg->type = 0;
            msg->check_sum = 0;
            msg->check_sum = jlos_internet_protocol_provider_check_sum((uint16_t *)msg, size);
            return true;
    }
    return false;
}

void jlos_icmp_request_echo_reply(jlos_icmp_t* self, uint32_t ip_be)
{
    jlos_icmp_message_t icmp;
    icmp.type = 8;
    icmp.code = 0;
    icmp.data = 0x3713;
    icmp.check_sum = 0;
    icmp.check_sum = jlos_internet_protocol_provider_check_sum((uint16_t *)&icmp, sizeof(jlos_icmp_message_t));
    jlos_internet_protocol_handler_send(&self->base_handler, ip_be, (uint8_t *)&icmp, sizeof(jlos_icmp_message_t));
}
