#ifndef __JLOS_NET_ICMP_H
#define __JLOS_NET_ICMP_H

#include <net/ipv4.h>

namespace JLOS {
namespace Net {
struct InternetControlMessageProtocolMessage {
     uint8_t type;
     uint8_t code;
     uint16_t checkSum;
     uint32_t data;
} __attribute__((packed));

class InternetControlMessageProtocol : public InternetProtocolHandler {
public:
     InternetControlMessageProtocol(InternetProtocolProvider *backend);
     ~InternetControlMessageProtocol();

     bool OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
          uint8_t *internetProtocolPayload, uint32_t size);
     void RequestEchoReply(uint32_t ip_be);
};
}
}
#endif
