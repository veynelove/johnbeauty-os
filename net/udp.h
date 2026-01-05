#ifndef __JLOS_NET_UDP_H
#define __JLOS_NET_UDP_H

#include <net/etherframe.h>
#include <net/ipv4.h>

namespace JLOS {
namespace Net {
struct UserDatagramProtocolHeader {
     uint16_t srcPort;
     uint32_t srcIP;
     uint16_t dstPort;
     uint32_t dstIP;
     uint16_t length;
     uint16_t checksum;
} __attribute__((packed));

class UserDatagramProtocolSocket;
class UserDatagramProtocolProvider;

class UserDatagramProtocolHandler {
public:
     UserDatagramProtocolHandler();
     ~UserDatagramProtocolHandler();

     virtual void HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket *socket,
          uint8_t *data, uint16_t size);
};

class UserDatagramProtocolSocket {
friend class UserDatagramProtocolProvider;
protected:
     uint16_t remotePort;
     uint32_t remoteIP;
     uint16_t localPort;
     uint32_t localIP;
     UserDatagramProtocolProvider *backend;
     UserDatagramProtocolHandler *handler;
     bool listening;

public:
     UserDatagramProtocolSocket(UserDatagramProtocolProvider *backend);
     ~UserDatagramProtocolSocket();
     virtual void HandleUserDatagramProtocolMessage(uint8_t *data, uint16_t size);
     virtual void Send(uint8_t *data, uint16_t size);
     virtual void Disconnect();
};

class UserDatagramProtocolProvider : public InternetProtocolHandler {
protected:
     UserDatagramProtocolSocket *sockets[65535];
     uint16_t numSockets;
     uint16_t freePort;

public:
     UserDatagramProtocolProvider(InternetProtocolProvider *backend);
     ~UserDatagramProtocolProvider();

     virtual bool OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
          uint8_t *internetProtocolPayload, uint32_t size);
     
     virtual UserDatagramProtocolSocket *Connect(uint32_t ip, uint16_t port);
     virtual UserDatagramProtocolSocket *Listen(uint16_t port);
     virtual void Disconnect(UserDatagramProtocolSocket *socket);
     virtual void Send(UserDatagramProtocolSocket *socket, uint8_t *data, uint16_t size);
     virtual void Bind(UserDatagramProtocolSocket *socket, UserDatagramProtocolHandler *handler);
};
}
}
#endif
