#ifndef __JLOS_NET_ETHERFRAME_H
#define __JLOS_NET_ETHERFRAME_H

#include <common/types.h>
#include <drivers/amd_am79c973.h>
#include <kernel/memorymanagerment.h>

namespace JLOS {
namespace Net {
#define SWAP_ENDIAN_16(x) ((((x) & 0x00FF) << 8) \
     | (((x) & 0xFF00) >> 8))

struct EtherFrameHeader {
     uint64_t dstMAC_BE{48};
     uint64_t srcMAC_BE{48};
     uint64_t etherType_BE;
} __attribute__((packed));

typedef uint32_t EtherFrameFooter;
class EtherFrameProvider;

class EtherFrameHandler {
protected:
     EtherFrameProvider *backend;
     uint16_t etherType_BE;

public:
     EtherFrameHandler(EtherFrameProvider *backend, uint16_t etherType_BE);
     ~EtherFrameHandler();

     virtual bool OnEtherFrameReceived(uint8_t *etherframePayload, uint32_t size);
     void Send(uint64_t dstMAC_BE, uint16_t etherType_BE, uint8_t *buffer, uint32_t size);
     uint32_t GetIPAddress();
};

class EtherFrameProvider : public Drivers::RawDataHandler {
friend class EtherFrameHandler;
protected:
     EtherFrameHandler *handlers[65535];
public:
     EtherFrameProvider(Drivers::amd_am79c973 *backend);
     ~EtherFrameProvider();
     
     bool OnRawDataReceived(uint8_t *buffer, uint32_t size);
     void Send(uint64_t dstMAC_BE, uint16_t etherType_BE, uint8_t *buffer, uint32_t size);

     uint64_t GetMACAddress();
     uint32_t GetIPAddress();
};
}
}
#endif
