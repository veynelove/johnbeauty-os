#include <net/tcp.h>
#include <kernel/memorymanagerment.h>

namespace JLOS {
namespace Net {
TransmissionControlProtocolHandler::TransmissionControlProtocolHandler(){}

TransmissionControlProtocolHandler::~TransmissionControlProtocolHandler(){}

bool TransmissionControlProtocolHandler::HandleTransmissionControlProtocolMessage(
     TransmissionControlProtocolSocket *socket, uint8_t *data, uint16_t size)
{
     return true;
}

TransmissionControlProtocolSocket::TransmissionControlProtocolSocket(TransmissionControlProtocolProvider *backend)
: backend(backend), handler(0), state(CLOSED){}

TransmissionControlProtocolSocket::~TransmissionControlProtocolSocket(){}
bool TransmissionControlProtocolSocket::HandleTransmissionControlProtocolMessage(uint8_t *data, uint16_t size)
{
     if (handler) {
          return handler->HandleTransmissionControlProtocolMessage(this, data, size);
     }
     return false;
}

void TransmissionControlProtocolSocket::Send(uint8_t *data, uint16_t size)
{
     while (state != ESTABLISHED) {}
     backend->Send(this, data ,size, (PSH | ACK));
}

void TransmissionControlProtocolSocket::Disconnect()
{
     backend->Disconnect(this);
}

TransmissionControlProtocolProvider::TransmissionControlProtocolProvider(InternetProtocolProvider *backend)
: InternetProtocolHandler(backend, 0x06), numSockets(0), freePort(1024)
{
     for (int i = 0; i < 65535; i++) {
          sockets[i] = 0;
     }
}

TransmissionControlProtocolProvider::~TransmissionControlProtocolProvider(){}

bool TransmissionControlProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internetProtocolPayload, uint32_t size)
{
     if (size < 20) {
          return false;
     }
     TransmissionControlProtocolHeader *msg = (TransmissionControlProtocolHeader *)internetProtocolPayload;
     uint16_t localPort = msg->dstPort;
     uint16_t remotePort = msg->srcPort;

     TransmissionControlProtocolSocket *socket = 0;
     for (uint16_t i = 0; i < numSockets && socket == 0; i++) {
          if (sockets[i]->localPort == msg->dstPort && sockets[i]->localIP == dstIP_BE
               && sockets[i]->state == LISTEN && ((msg->flags) & (SYN | ACK) == SYN)) {
               socket = sockets[i];
          }
          else if (sockets[i]->localPort == msg->dstPort && sockets[i]->localIP == dstIP_BE
               && sockets[i]->remotePort == msg->srcPort && sockets[i]->remoteIP == srcIP_BE) {
               socket = sockets[i];
          }
     }

     bool reset = false;
     if (socket && msg->flags && RST) {
          socket->state = CLOSED;
     }
     if (socket && socket->state != CLOSED) {
          switch ((msg->flags) & (SYN | ACK | FIN)) {
               case SYN :
                    if (socket->state == LISTEN) {
                         socket->state = SYN_RECEIVED;
                         socket->remotePort = msg->srcPort;
                         socket->remoteIP = srcIP_BE;
                         socket->acknowledgementNumber = SWAP_ENDIAN_32(msg->sequenceNumber) + 1;
                         socket->sequenceNumber = 0xbeefcafe;
                         Send(socket, 0, 0, (SYN | ACK));
                         socket->sequenceNumber++;
                    } else {
                         reset = true;
                    }
                    break;
               case (SYN | ACK) :
                    if (socket->state == SYN_SENT) {
                         socket->state = ESTABLISHED;
                         socket->acknowledgementNumber = SWAP_ENDIAN_32(msg->sequenceNumber) + 1;
                         socket->sequenceNumber++;
                         Send(socket, 0, 0, ACK);
                    } else {
                         reset = true;
                    }
                    break;
               case (SYN | FIN) :
               case (SYN | FIN | ACK) :
                    reset = true;
                    break;
               case FIN :
               case (FIN | ACK) :
                    switch (socket->state) {
                         case ESTABLISHED :
                              socket->state = CLOSE_WAIT;
                              socket->acknowledgementNumber++;
                              Send(socket, 0, 0, ACK);
                              Send(socket, 0, 0, (FIN | ACK));
                              break;
                         case CLOSE_WAIT :
                              socket->state = CLOSED;
                              break;
                         case (FIN_WAIT1 | FIN_WAIT2) :
                              socket->state = CLOSED;
                              socket->acknowledgementNumber++;
                              Send(socket, 0, 0, ACK);
                              break;
                         default :
                              reset = true;
                    }
                    break;
               case ACK :
                    switch (socket->state) {
                         case SYN_RECEIVED :
                              socket->state = ESTABLISHED;
                              return false;
                         case FIN_WAIT1 :
                              socket->state = FIN_WAIT2;
                              return false;
                         case CLOSE_WAIT :
                              socket->state = CLOSED;
                    }
                    break;
               default :
                    if (SWAP_ENDIAN_32(msg->sequenceNumber) == socket->acknowledgementNumber) {
                         reset = !socket->HandleTransmissionControlProtocolMessage((internetProtocolPayload +
                              msg->headerSize32 * 4), (size - msg->headerSize32 * 4));
                         if (!reset) {
                              socket->acknowledgementNumber += (size - msg->headerSize32 * 4);
                              Send(socket, 0, 0, ACK);
                         }
                    } else {
                         // data in wrong order
                         reset = true;
                    }
          }
     }
     if (reset) {
          if (socket) {
               Send(socket, 0, 0, RST);
          } else {
               TransmissionControlProtocolSocket socket1(this);
               socket1.remotePort = msg->srcPort;
               socket1.remoteIP = srcIP_BE;
               socket1.localPort = msg->dstPort;
               socket1.localIP = dstIP_BE;
               socket1.sequenceNumber = SWAP_ENDIAN_32(msg->acknowledgementNumber);
               socket1.acknowledgementNumber = SWAP_ENDIAN_32(msg->sequenceNumber) + 1;
               Send(&socket1, 0, 0, RST);
               return true;
          }
     }
     if (socket && socket->state == CLOSED) {
          for (uint16_t i = 0; i < numSockets && socket == 0; i++) {
               if (sockets[i] == socket) {
                    sockets[i] = sockets[--numSockets];
                    Kernel::MemoryManager::activeMemoryManager->free(socket);
                    break;
               }
          }
     }
     return false;
}

void TransmissionControlProtocolProvider::Send(TransmissionControlProtocolSocket *socket,
     uint8_t *data, uint16_t size, uint16_t flags)
{
     uint16_t totalLength = size + sizeof(TransmissionControlProtocolHeader);
     uint16_t lengthInclPHdr = totalLength + sizeof(TransmissionControlProtocolPseudoHeader);

     uint8_t *buffer = (uint8_t *)Kernel::MemoryManager::activeMemoryManager->malloc(lengthInclPHdr);
     
     TransmissionControlProtocolPseudoHeader *phdr = (TransmissionControlProtocolPseudoHeader *)buffer;
     TransmissionControlProtocolHeader *msg = (TransmissionControlProtocolHeader *)(buffer +
          sizeof(TransmissionControlProtocolPseudoHeader));
     
     uint8_t *buffer2 = buffer + sizeof(TransmissionControlProtocolHeader) +
          sizeof(TransmissionControlProtocolPseudoHeader);

     msg->headerSize32 = sizeof(TransmissionControlProtocolHandler) / 4;
     msg->srcPort = socket->localPort;
     msg->dstPort = socket->remotePort;
     msg->acknowledgementNumber = SWAP_ENDIAN_32(socket->acknowledgementNumber);
     msg->sequenceNumber = SWAP_ENDIAN_32(socket->sequenceNumber);
     msg->reserved = 0;
     msg->flags = flags;
     msg->windowSize = 0xFFFF;
     msg->urgentPtr = 0;
     msg->options = ((flags & SYN) != 0) ? 0xB4050402 : 0;
     
     socket->sequenceNumber += size;

     for (int i = 0; i < size; i++) {
          buffer2[i] = data[i];
     }
     phdr->srcIP = socket->localIP;
     phdr->dstIP = socket->remoteIP;
     phdr->protocol = 0x0600;
     phdr->totalLength = SWAP_ENDIAN_16(totalLength);
     msg->checksum = 0;
     msg->checksum = InternetProtocolProvider::CheckSum((uint16_t *)buffer, lengthInclPHdr);
     InternetProtocolHandler::Send(socket->remoteIP, (uint8_t *)msg, totalLength);
     Kernel::MemoryManager::activeMemoryManager->free(buffer);
}


TransmissionControlProtocolSocket *TransmissionControlProtocolProvider::Connect(uint32_t ip, uint16_t port)
{
     TransmissionControlProtocolSocket *socket =
          (TransmissionControlProtocolSocket *)Kernel::MemoryManager::activeMemoryManager->malloc(
               sizeof(TransmissionControlProtocolSocket));
     if (socket) {
          new (socket) TransmissionControlProtocolSocket(this);
          socket->remotePort = port;
          socket->remoteIP = ip;
          socket->localPort = freePort++;
          socket->localIP = backend->GetIPAddress();
          socket->remotePort = SWAP_ENDIAN_16(socket->remotePort);
          socket->localPort = SWAP_ENDIAN_16(socket->localPort);
          sockets[numSockets++] = socket;
          socket->state = SYN_SENT;
          socket->sequenceNumber = 0xbeefcafe;
          Send(socket, 0, 0, SYN);
     }
     return socket;
}

void TransmissionControlProtocolProvider::Disconnect(TransmissionControlProtocolSocket *socket)
{
     socket->state = FIN_WAIT1;
     Send(socket, 0, 0, FIN + ACK);
     socket->sequenceNumber++;
}

TransmissionControlProtocolSocket *TransmissionControlProtocolProvider::Listen(uint16_t port)
{
     TransmissionControlProtocolSocket *socket =
          (TransmissionControlProtocolSocket *)Kernel::MemoryManager::activeMemoryManager->malloc(
               sizeof(TransmissionControlProtocolSocket));
     if (socket) {
          new (socket) TransmissionControlProtocolSocket(this);
          socket->state = LISTEN;
          socket->localIP = backend->GetIPAddress();
          socket->localPort = SWAP_ENDIAN_16(port);
          sockets[numSockets++] = socket;
     }
     return socket;
}

void TransmissionControlProtocolProvider::Bind(TransmissionControlProtocolSocket *socket,
     TransmissionControlProtocolHandler *handler)
{
     socket->handler = handler;
}
}
}
