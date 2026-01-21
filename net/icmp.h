#ifndef __JLOS_NET_ICMP_H
#define __JLOS_NET_ICMP_H

#include <net/ipv4.h>

namespace JLOS {
namespace Net {
struct internet_control_message_protocol_message {
     uint8_t m_type;
     uint8_t m_code;
     uint16_t m_check_sum;
     uint32_t m_data;
} __attribute__((packed));

class internet_control_message_protocol : public internet_protocol_handler {
public:
     internet_control_message_protocol(internet_protocol_provider *backend);
     ~internet_control_message_protocol();

     bool on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
          uint8_t *internet_protocol_payload, uint32_t m_size);
     void request_echo_reply(uint32_t ip_be);
};
}
}
#endif
