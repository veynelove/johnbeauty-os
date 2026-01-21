#include <net/ipv4.h>
namespace JLOS {
namespace Net {
internet_protocol_handler::internet_protocol_handler(internet_protocol_provider *backend, uint8_t m_protocol)
: backend(backend), m_ip_protocol(m_protocol)
{
     backend->handlers[m_protocol] = this;
}

internet_protocol_handler::~internet_protocol_handler()
{
     if (backend->handlers[m_ip_protocol] == this) {
          backend->handlers[m_ip_protocol] = nullptr;
     }
}

bool internet_protocol_handler::on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internet_protocol_payload, uint32_t m_size)
{
     return false;
}

void internet_protocol_handler::send(uint32_t dstIP_BE, uint8_t *internet_protocol_payload,
     uint32_t m_size)
{
     backend->send(dstIP_BE, m_ip_protocol, internet_protocol_payload, m_size);
}

internet_protocol_provider::internet_protocol_provider(ether_frame_provider *backend,
     address_resolution_protocol *arp, uint32_t m_gateway_ip, uint32_t m_subnet_mask)
: ether_frame_handler(backend, 0x800), arp(arp), m_gateway_ip(m_gateway_ip), m_subnet_mask(m_subnet_mask)
{
     for (int i = 0; i < 255; i++) {
          handlers[i] = nullptr;
     }
}

internet_protocol_provider::~internet_protocol_provider(){}

bool internet_protocol_provider::on_ether_frame_received(uint8_t *etherframe_payload, uint32_t m_size)
{
     if (m_size < sizeof(internet_protocol_v4_message)) {
          return false;
     }
     internet_protocol_v4_message *ip_message = (internet_protocol_v4_message *)etherframe_payload;
     bool send_back = false;
     if (ip_message->m_dst_ip == backend->get_ip_address()) {
          int m_length = ip_message->m_total_length;
          if (m_length > m_size) {
               m_length = m_size; // defend the hard bleed attack
          }
          if (handlers[ip_message->m_protocol]) {
               send_back = handlers[ip_message->m_protocol]->on_internet_protocol_received(
                   ip_message->m_src_ip, ip_message->m_dst_ip, 
                   etherframe_payload + 4 * ip_message->header_length, m_length - 4 * ip_message->header_length);
          }
     }
     if (send_back) {
          uint32_t temp = ip_message->m_dst_ip;
          ip_message->m_dst_ip = ip_message->m_src_ip;
          ip_message->m_src_ip = temp;

          ip_message->m_time_to_live = 0x40;
          ip_message->m_checksum = 0;
          ip_message->m_checksum = m_check_sum((uint16_t *)ip_message, 4 * ip_message->header_length);
     }
     return send_back;
}

void internet_protocol_provider::send(uint32_t dstIP_BE, uint8_t m_protocol, uint8_t *m_data, uint32_t m_size)
{
     uint8_t *buffer = 
          (uint8_t *)Kernel::memory_manager::active_memory_manager->malloc(sizeof(internet_protocol_v4_message) + m_size);
     internet_protocol_v4_message *message = (internet_protocol_v4_message *)buffer;
     message->version = 4;
     message->header_length = sizeof(internet_protocol_v4_message)/4;
     message->m_tos = 0;
     message->m_total_length = m_size + sizeof(internet_protocol_v4_message);
     message->m_total_length = SWAP_ENDIAN_16(message->m_total_length);
     message->m_ident = 0x0100;
     message->flags_and_offset = 0x0040;
     message->m_time_to_live = 0x40;
     message->m_protocol = m_protocol;
     
     message->m_dst_ip = dstIP_BE;
     message->m_src_ip = backend->get_ip_address();

     message->m_checksum = 0;
     message->m_checksum = m_check_sum((uint16_t *)message, sizeof(internet_protocol_v4_message));

     uint8_t *data_buffer = buffer + sizeof(internet_protocol_v4_message);
     for (int i = 0; i < m_size; i++) {
          data_buffer[i] = m_data[i];
     }
     uint32_t route = dstIP_BE;
     if ((dstIP_BE & m_subnet_mask) != (message->m_src_ip & m_subnet_mask)) {
          route = m_gateway_ip;
     }
     backend->send(arp->resolve(route),
          this->m_etherType_BE, buffer, sizeof(internet_protocol_v4_message) + m_size);
     Kernel::memory_manager::active_memory_manager->free(buffer);
}

uint16_t internet_protocol_provider::m_check_sum(uint16_t *m_data, uint32_t length_in_bytes)
{
     uint32_t temp = 0;
     for (int i = 0; i < length_in_bytes/2; i++) {
          temp += SWAP_ENDIAN_16(m_data[i]);
     }
     if (length_in_bytes % 2) {
          temp += (uint16_t)(((char *)m_data)[length_in_bytes - 1]) << 8;
     }
     while (temp & 0xFFFF0000) {
          temp = (temp & 0xFFFF) + (temp >> 16);
     }
     return SWAP_ENDIAN_16(~temp);
}
}
}
