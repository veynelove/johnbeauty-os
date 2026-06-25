#include <net/icmp.h>

extern void printf(const char *str);
extern void printf_hex(uint8_t);

void jlos_icmp_init(jlos_icmp_t* self, jlos_internet_protocol_provider_t *backend)
{
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x01);
    self->base_handler.on_internet_protocol_received = (bool (*)(jlos_internet_protocol_handler_t*, uint32_t, uint32_t, uint8_t*, uint32_t))jlos_icmp_on_internet_protocol_received;
}

void jlos_icmp_destroy(jlos_icmp_t* self)
{
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

bool jlos_icmp_on_internet_protocol_received(jlos_icmp_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size)
{
    if (m_size < sizeof(jlos_icmp_message_t)) {
        return false;
    }
    jlos_icmp_message_t *msg = (jlos_icmp_message_t *)internet_protocol_payload;
    switch (msg->m_type) {
        case 0:
            printf("ping response from ");
            printf_hex(srcIP_BE & 0xFF);
            printf(".");
            printf_hex((srcIP_BE >> 8) & 0xFF);
            printf(".");
            printf_hex((srcIP_BE >> 16) & 0xFF);
            printf(".");
            printf_hex((srcIP_BE >> 24) & 0xFF);
            printf("\n");
            break;
        case 8:
            msg->m_type = 0;
            msg->m_check_sum = 0;
            msg->m_check_sum = jlos_internet_protocol_provider_check_sum((uint16_t *)msg, sizeof(jlos_icmp_message_t));
            return true;
    }
    return false;
}

void jlos_icmp_request_echo_reply(jlos_icmp_t* self, uint32_t ip_be)
{
    jlos_icmp_message_t icmp;
    icmp.m_type = 8;
    icmp.m_code = 0;
    icmp.m_data = 0x3713;
    icmp.m_check_sum = 0;
    icmp.m_check_sum = jlos_internet_protocol_provider_check_sum((uint16_t *)&icmp, sizeof(jlos_icmp_message_t));
    jlos_internet_protocol_handler_send(&self->base_handler, ip_be, (uint8_t *)&icmp, sizeof(jlos_icmp_message_t));
}