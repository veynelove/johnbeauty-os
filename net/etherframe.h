#ifndef __JLOS_NET_ETHERFRAME_H
#define __JLOS_NET_ETHERFRAME_H

#include <common/types.h>
#include <drivers/amd_am79c973.h>
#include <kernel/memory_manager.h>

namespace JLOS {
namespace Net {
#define SWAP_ENDIAN_16(m_x) ((((m_x) & 0x00FF) << 8) \
     | (((m_x) & 0xFF00) >> 8))

#define SWAP_ENDIAN_32(m_x) (((m_x) & 0xFF000000 >> 24) \
     | ((m_x) & 0x00FF0000 >> 8) | ((m_x) & 0x0000FF00 << 8) \
     | ((m_x) & 0x000000FF << 24))

struct ether_frame_header {
     uint64_t dstMAC_BE{48};
     uint64_t srcMAC_BE{48};
     uint64_t m_etherType_BE;
} __attribute__((packed));

typedef uint32_t ether_frame_footer;
class ether_frame_provider;

class ether_frame_handler {
protected:
     ether_frame_provider *backend;
     uint16_t m_etherType_BE;

public:
     ether_frame_handler(ether_frame_provider *backend, uint16_t m_etherType_BE);
     ~ether_frame_handler();

     virtual bool on_ether_frame_received(uint8_t *etherframe_payload, uint32_t m_size);
     void send(uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size);
     uint32_t get_ip_address();
};

class ether_frame_provider : public Drivers::rawdata_handler {
friend class ether_frame_handler;
protected:
     ether_frame_handler *handlers[65535];
public:
     ether_frame_provider(Drivers::amd_am79c973 *backend);
     ~ether_frame_provider();
     
     bool on_raw_data_received(uint8_t *buffer, uint32_t m_size);
     void send(uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size);

     uint64_t get_mac_address();
     uint32_t get_ip_address();
};
}
}
#endif
