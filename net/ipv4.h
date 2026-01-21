#ifndef __JLOS_NET_IPV4_H
#define __JLOS_NET_IPV4_H

#include <net/etherframe.h>
#include <net/arp.h>

namespace JLOS {
namespace Net {
struct internet_protocol_v4_message {
     uint8_t header_length{4};
     uint8_t version{4};
     uint8_t m_tos;
     uint16_t m_total_length;
     uint16_t m_ident;
     uint16_t flags_and_offset{13};
     uint8_t m_time_to_live;
     uint8_t m_protocol;
     uint16_t m_checksum;
     uint32_t m_src_ip;
     uint32_t m_dst_ip;
} __attribute__((packed));

class internet_protocol_provider;

class internet_protocol_handler {
protected:
     internet_protocol_provider *backend;
     uint8_t m_ip_protocol;

public:
     internet_protocol_handler(internet_protocol_provider *backend, uint8_t m_protocol);
     ~internet_protocol_handler();

     virtual bool on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
          uint8_t *internet_protocol_payload, uint32_t m_size);
     void send(uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size);
};

class internet_protocol_provider : public ether_frame_handler {
friend class internet_protocol_handler;
protected:
     internet_protocol_handler *handlers[255];
     address_resolution_protocol *arp;
     uint32_t m_gateway_ip;
     uint32_t m_subnet_mask;

public:
     internet_protocol_provider(ether_frame_provider *backend, address_resolution_protocol *arp,
          uint32_t m_gateway_ip, uint32_t m_subnet_mask);
     ~internet_protocol_provider();
     
     bool on_ether_frame_received(uint8_t *etherframe_payload, uint32_t m_size);
     void send(uint32_t dstIP_BE, uint8_t m_protocol, uint8_t *m_data, uint32_t m_size);

     static uint16_t m_check_sum(uint16_t *m_data, uint32_t length_in_bytes);
};
}
}
#endif
