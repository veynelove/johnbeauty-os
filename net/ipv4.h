#ifndef __JLOS_NET_IPV4_H
#define __JLOS_NET_IPV4_H

#include <net/etherframe.h>
#include <net/arp.h>

namespace JLOS {
namespace Net {
struct InternetProtocolV4Message {
     uint8_t headerLength{4};
     uint8_t version{4};
     uint8_t tos;
     uint16_t totalLength;
     uint16_t ident;
     uint16_t flagsAndOffset{13};
     uint8_t timeToLive;
     uint8_t protocol;
     uint16_t checksum;
     uint32_t srcIP;
     uint32_t dstIP;
} __attribute__((packed));

class InternetProtocolProvider;

class InternetProtocolHandler {
protected:
     InternetProtocolProvider *backend;
     uint8_t ip_protocol;

public:
     InternetProtocolHandler(InternetProtocolProvider *backend, uint8_t protocol);
     ~InternetProtocolHandler();

     virtual bool OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
          uint8_t *internetProtocolPayload, uint32_t size);
     void Send(uint32_t dstIP_BE, uint8_t *internetProtocolPayload, uint32_t size);
};

class InternetProtocolProvider : public EtherFrameHandler {
friend class InternetProtocolHandler;
protected:
     InternetProtocolHandler *handlers[255];
     AddressResolutionProtocol *arp;
     uint32_t gatewayIP;
     uint32_t subnetMask;

public:
     InternetProtocolProvider(EtherFrameProvider *backend, AddressResolutionProtocol *arp,
          uint32_t gatewayIP, uint32_t subnetMask);
     ~InternetProtocolProvider();
     
     bool OnEtherFrameReceived(uint8_t *etherframePayload, uint32_t size);
     void Send(uint32_t dstIP_BE, uint8_t protocol, uint8_t *data, uint32_t size);

     static uint16_t CheckSum(uint16_t *data, uint32_t lengthInBytes);
};
}
}
#endif
