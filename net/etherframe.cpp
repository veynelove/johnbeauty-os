#include <net/etherframe.h>

namespace JLOS {
namespace Net {
ether_frame_handler::ether_frame_handler(ether_frame_provider *backend, uint16_t m_etherType_BE)
: m_etherType_BE(SWAP_ENDIAN_16(m_etherType_BE)), backend(backend)
{
     backend->handlers[m_etherType_BE] = this;
}

ether_frame_handler::~ether_frame_handler()
{
     if (backend->handlers[m_etherType_BE] == this) {
          backend->handlers[m_etherType_BE] = nullptr;
     }
}

bool ether_frame_handler::on_ether_frame_received(uint8_t *etherframe_payload, uint32_t m_size)
{
     return false;
}

void ether_frame_handler::send(uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size)
{
     backend->send(dstMAC_BE, m_etherType_BE, buffer, m_size);
}

uint32_t ether_frame_handler::get_ip_address()
{
     return backend->get_ip_address();
}

ether_frame_provider::ether_frame_provider(Drivers::amd_am79c973 *backend)
: rawdata_handler(backend)
{
     for (uint32_t i = 0; i < 65535; i++) {
          handlers[i] = 0;
     }
}

ether_frame_provider::~ether_frame_provider(){}

bool ether_frame_provider::on_raw_data_received(uint8_t *buffer, uint32_t m_size)
{
     if (m_size < sizeof(ether_frame_header)) {
          return false;
     }
     ether_frame_header *frame = (ether_frame_header *)buffer;
     bool send_back = false;
     if (frame->dstMAC_BE == 0xFFFFFFFFFFFF
          || frame->dstMAC_BE == backend->get_mac_address()) {
          if (handlers[frame->m_etherType_BE]) {
               send_back = handlers[frame->m_etherType_BE]->on_ether_frame_received(
                    buffer + sizeof(ether_frame_header), m_size - sizeof(ether_frame_handler));
          }
     }
     if (send_back) {
          frame->dstMAC_BE = frame->srcMAC_BE;
          frame->srcMAC_BE = backend->get_mac_address();
     }
     return send_back;
}

void ether_frame_provider::send(uint64_t dstMAC_BE, uint16_t m_etherType_BE, uint8_t *buffer, uint32_t m_size)
{
     uint8_t *buffer2 = 
          (uint8_t *)Kernel::memory_manager::active_memory_manager->malloc(sizeof(ether_frame_header) + m_size);
     ether_frame_header *frame = (ether_frame_header *)buffer2;
     frame->dstMAC_BE = dstMAC_BE;
     frame->srcMAC_BE = backend->get_mac_address();
     frame->m_etherType_BE = m_etherType_BE;

     uint8_t *src = buffer;
     uint8_t *dst = buffer2 + sizeof(ether_frame_header);
     for (uint32_t i = 0; i < m_size; i++) {
          dst[i] = src[i];
     }
     backend->send(buffer2, m_size + sizeof(ether_frame_header));
     Kernel::memory_manager::active_memory_manager->free(buffer2);
}

uint64_t ether_frame_provider::get_mac_address()
{
     return backend->get_mac_address();
}

uint32_t ether_frame_provider::get_ip_address()
{
     return backend->get_ip_address();
}
}
}