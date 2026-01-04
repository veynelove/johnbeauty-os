#ifndef __JLOS_NET_ARP_H
#define __JLOS_NET_ARP_H

#include <net/etherframe.h>

namespace JLOS {
namespace Net {
struct AddressResolutionProtocolMessage {
     uint16_t hardwareType;
     uint16_t protocol;
     uint16_t hardwareAddressSize; //6
     uint8_t protocolAddressSize; //4
     uint16_t command;

     uint64_t srcMAC{48};
     uint32_t srcIP;
     uint64_t dstMAC{48};
     uint32_t dstIP;
} __attribute__((packed));

class AddressResolutionProtocol : public EtherFrameHandler {
private:
     uint32_t IPCache[128];
     uint64_t MACCache[128];
     int numCacheEntries;

public:
     AddressResolutionProtocol(EtherFrameProvider *backend);
     ~AddressResolutionProtocol();

     bool OnEtherFrameReceived(uint8_t *etherframePayload, uint32_t size);

     void RequestMACAddress(uint32_t IP_BE);
     uint64_t GetMACFromCache(uint32_t IP_BE);
     uint64_t Resolve(uint32_t IP_BE);
     void BroadcastMACAddress(uint32_t IP_BE);
};
}
}

#endif
