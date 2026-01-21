#ifndef __JLOS_NET_ARP_H
#define __JLOS_NET_ARP_H

#include <net/etherframe.h>

namespace JLOS {
namespace Net {
struct address_resolution_protocol_message {
     uint16_t m_hardware_type;
     uint16_t m_protocol;
     uint16_t m_hardware_address_size; //6
     uint8_t m_protocol_address_size; //4
     uint16_t m_command;

     uint64_t src_mac{48};
     uint32_t m_src_ip;
     uint64_t dst_mac{48};
     uint32_t m_dst_ip;
} __attribute__((packed));

class address_resolution_protocol : public ether_frame_handler {
private:
     uint32_t ip_cache[128];
     uint64_t mac_cache[128];
     int m_num_cache_entries;

public:
     address_resolution_protocol(ether_frame_provider *backend);
     ~address_resolution_protocol();

     bool on_ether_frame_received(uint8_t *etherframe_payload, uint32_t m_size);

     void request_mac_address(uint32_t IP_BE);
     uint64_t get_mac_from_cache(uint32_t IP_BE);
     uint64_t resolve(uint32_t IP_BE);
     void broadcast_mac_address(uint32_t IP_BE);
};
}
}

#endif
