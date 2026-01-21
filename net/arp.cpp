#include <net/arp.h>

namespace JLOS {
namespace Net {
address_resolution_protocol::address_resolution_protocol(ether_frame_provider *backend)
: ether_frame_handler(backend, 0x806), m_num_cache_entries(0){}

address_resolution_protocol::~address_resolution_protocol(){}

bool address_resolution_protocol::on_ether_frame_received(uint8_t *etherframe_payload, uint32_t m_size)
{
     if (m_size < sizeof(address_resolution_protocol_message)) {
          return false;
     }
     address_resolution_protocol_message *arp =
          (address_resolution_protocol_message *)etherframe_payload;
     if (arp->m_hardware_type == 0x0100) {
          if (arp->m_protocol == 0x0008 && arp->m_hardware_address_size == 6
               &&arp->m_protocol_address_size == 4 && arp->m_dst_ip == backend->get_ip_address()) {
               switch (arp->m_command) {
                    case 0x0100 : // request
                         arp->m_command = 0x0200;
                         arp->m_dst_ip = arp->m_src_ip;
                         arp->dst_mac = arp->src_mac;
                         arp->m_src_ip = backend->get_ip_address();
                         arp->src_mac = backend->get_mac_address();
                         return true;
                    case 0x0200 : // response
                         if (m_num_cache_entries < 128) {
                              ip_cache[m_num_cache_entries] = arp->m_src_ip;
                              mac_cache[m_num_cache_entries] = arp->src_mac;
                              m_num_cache_entries++;
                         }
                         break;
               }
          }
     }
     return false;
}

void address_resolution_protocol::broadcast_mac_address(uint32_t IP_BE)
{
     address_resolution_protocol_message arp;
     arp.m_hardware_type = 0x0100; // ethernet
     arp.m_protocol = 0x0008; // ipv4
     arp.m_hardware_address_size = 6; //mac
     arp.m_protocol_address_size = 4; //ipv4
     arp.m_command = 0x0200; // response

     arp.src_mac = backend->get_mac_address();
     arp.m_src_ip = backend->get_ip_address();
     arp.dst_mac = resolve(IP_BE); //broadcast
     arp.m_dst_ip = IP_BE;
     this->send(arp.dst_mac, m_etherType_BE, (uint8_t *)&arp, sizeof(address_resolution_protocol_message));
}

void address_resolution_protocol::request_mac_address(uint32_t IP_BE)
{
     address_resolution_protocol_message arp;
     arp.m_hardware_type = 0x0100; // ethernet
     arp.m_protocol = 0x0008; // ipv4
     arp.m_hardware_address_size = 6; //mac
     arp.m_protocol_address_size = 4; //ipv4
     arp.m_command = 0x0100; // request

     arp.src_mac = backend->get_mac_address();
     arp.m_src_ip = backend->get_ip_address();
     arp.dst_mac = 0xFFFFFFFFFFFF; //broadcast
     arp.m_dst_ip = IP_BE;
     this->send(arp.dst_mac, m_etherType_BE, (uint8_t *)&arp, sizeof(address_resolution_protocol_message));
}

uint64_t address_resolution_protocol::get_mac_from_cache(uint32_t IP_BE)
{
     for (int i = 0; i < m_num_cache_entries; i++) {
          if (ip_cache[i] == IP_BE) {
               return mac_cache[i];
          }
     }
     return 0xFFFFFFFFFFFF; //broadcast m_address
}

uint64_t address_resolution_protocol::resolve(uint32_t IP_BE)
{
     uint64_t result = get_mac_from_cache(IP_BE);
     if (result == 0xFFFFFFFFFFFF) {
          request_mac_address(IP_BE);
     }
     while (result == 0xFFFFFFFFFFFF) { // possible infinite loop
          result = get_mac_from_cache(IP_BE);
     }
     return result;
}
}
}
