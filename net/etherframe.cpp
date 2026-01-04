#include <net/etherframe.h>

namespace JLOS {
namespace Net {
EtherFrameHandler::EtherFrameHandler(EtherFrameProvider *backend, uint16_t etherType_BE)
: etherType_BE(SWAP_ENDIAN_16(etherType_BE)), backend(backend)
{
     backend->handlers[etherType_BE] = this;
}

EtherFrameHandler::~EtherFrameHandler()
{
     if (backend->handlers[etherType_BE] == this) {
          backend->handlers[etherType_BE] = nullptr;
     }
}

bool EtherFrameHandler::OnEtherFrameReceived(uint8_t *etherframePayload, uint32_t size)
{
     return false;
}

void EtherFrameHandler::Send(uint64_t dstMAC_BE, uint16_t etherType_BE, uint8_t *buffer, uint32_t size)
{
     backend->Send(dstMAC_BE, etherType_BE, buffer, size);
}

EtherFrameProvider::EtherFrameProvider(Drivers::amd_am79c973 *backend)
: RawDataHandler(backend)
{
     for (uint32_t i = 0; i < 65535; i++) {
          handlers[i] = 0;
     }
}

EtherFrameProvider::~EtherFrameProvider(){}

bool EtherFrameProvider::OnRawDataReceived(uint8_t *buffer, uint32_t size)
{
     if (size < sizeof(EtherFrameHeader)) {
          return false;
     }
     EtherFrameHeader *frame = (EtherFrameHeader *)buffer;
     bool sendBack = false;
     if (frame->dstMAC_BE == 0xFFFFFFFFFFFF
          || frame->dstMAC_BE == backend->GetMACAddress()) {
          if (handlers[frame->etherType_BE]) {
               sendBack = handlers[frame->etherType_BE]->OnEtherFrameReceived(
                    buffer + sizeof(EtherFrameHeader), size - sizeof(EtherFrameHandler));
          }
     }
     if (sendBack) {
          frame->dstMAC_BE = frame->srcMAC_BE;
          frame->srcMAC_BE = backend->GetMACAddress();
     }
     return sendBack;
}

void EtherFrameProvider::Send(uint64_t dstMAC_BE, uint16_t etherType_BE, uint8_t *buffer, uint32_t size)
{
     uint8_t *buffer2 = 
          (uint8_t *)Kernel::MemoryManager::activeMemoryManager->malloc(sizeof(EtherFrameHeader) + size);
     EtherFrameHeader *frame = (EtherFrameHeader *)buffer2;
     frame->dstMAC_BE = dstMAC_BE;
     frame->srcMAC_BE = backend->GetMACAddress();
     frame->etherType_BE = etherType_BE;

     uint8_t *src = buffer;
     uint8_t *dst = buffer2 + sizeof(EtherFrameHeader);
     for (uint32_t i = 0; i < size; i++) {
          dst[i] = src[i];
     }
     backend->Send(buffer2, size + sizeof(EtherFrameHeader));
     Kernel::MemoryManager::activeMemoryManager->free(buffer2);
}

uint64_t EtherFrameProvider::GetMACAddress()
{
     return backend->GetMACAddress();
}

uint32_t EtherFrameProvider::GetIPAddress()
{
     return backend->GetIPAddress();
}
}
}