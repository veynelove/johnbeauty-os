#include <net/udp.h>
#include <kernel/memorymanagerment.h>

namespace JLOS {
namespace Net {
UserDatagramProtocolHandler::UserDatagramProtocolHandler(){}

UserDatagramProtocolHandler::~UserDatagramProtocolHandler(){}

void UserDatagramProtocolHandler::HandleUserDatagramProtocolMessage(
     UserDatagramProtocolSocket *socket, uint8_t *data, uint16_t size){}

UserDatagramProtocolSocket::UserDatagramProtocolSocket(UserDatagramProtocolProvider *backend)
: backend(backend), handler(0), listening(false){}

UserDatagramProtocolSocket::~UserDatagramProtocolSocket(){}
void UserDatagramProtocolSocket::HandleUserDatagramProtocolMessage(uint8_t *data, uint16_t size)
{
     if (handler) {
          handler->HandleUserDatagramProtocolMessage(this, data, size);
     }
}

void UserDatagramProtocolSocket::Send(uint8_t *data, uint16_t size)
{
     backend->Send(this, data ,size);
}

void UserDatagramProtocolSocket::Disconnect()
{
     backend->Disconnect(this);
}

UserDatagramProtocolProvider::UserDatagramProtocolProvider(InternetProtocolProvider *backend)
: InternetProtocolHandler(backend, 0x11), numSockets(0), freePort(1024)
{
     for (int i = 0; i < 65535; i++) {
          sockets[i] = 0;
     }
}

UserDatagramProtocolProvider::~UserDatagramProtocolProvider(){}

bool UserDatagramProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internetProtocolPayload, uint32_t size)
{
     if (size < sizeof(UserDatagramProtocolHeader)) {
          return false;
     }
     UserDatagramProtocolHeader *msg = (UserDatagramProtocolHeader *)internetProtocolPayload;
     uint16_t localPort = msg->dstPort;
     uint16_t remotePort = msg->srcPort;

     UserDatagramProtocolSocket *socket = 0;
     for (uint16_t i = 0; i < numSockets && socket == 0; i++) {
          if (sockets[i]->localPort == msg->dstPort && sockets[i]->localIP == dstIP_BE
               && sockets[i]->listening) {
               socket = sockets[i];
               socket->listening = false;
               socket->remotePort = msg->srcPort;
               socket->remoteIP = srcIP_BE;
          }
          else if (sockets[i]->localPort == msg->dstPort && sockets[i]->localIP == dstIP_BE
               && sockets[i]->remotePort == msg->srcPort && sockets[i]->remoteIP == srcIP_BE) {
               socket = sockets[i];
          }
     }
     if (socket) {
          socket->HandleUserDatagramProtocolMessage(internetProtocolPayload +
               sizeof(UserDatagramProtocolHeader), size - sizeof(UserDatagramProtocolHeader));
     }
     return false;
}

UserDatagramProtocolSocket *UserDatagramProtocolProvider::Connect(uint32_t ip, uint16_t port)
{
     UserDatagramProtocolSocket *socket =
          (UserDatagramProtocolSocket *)Kernel::MemoryManager::activeMemoryManager->malloc(
               sizeof(UserDatagramProtocolSocket));
     if (socket) {
          new (socket) UserDatagramProtocolSocket(this);
          socket->remotePort = port;
          socket->remoteIP = ip;
          socket->localPort = freePort++;
          socket->localIP = backend->GetIPAddress();
          socket->remotePort = SWAP_ENDIAN_16(socket->remotePort);
          socket->localPort = SWAP_ENDIAN_16(socket->localPort);
          sockets[numSockets++] = socket;
     }
     return socket;
}

UserDatagramProtocolSocket *UserDatagramProtocolProvider::Listen(uint16_t port)
{
     UserDatagramProtocolSocket *socket =
          (UserDatagramProtocolSocket *)Kernel::MemoryManager::activeMemoryManager->malloc(
               sizeof(UserDatagramProtocolSocket));
     if (socket) {
          new (socket) UserDatagramProtocolSocket(this);
          socket->listening = true;
          socket->localPort = port;
          socket->localIP = backend->GetIPAddress();
          socket->localPort = SWAP_ENDIAN_16(socket->localPort);
          sockets[numSockets++] = socket;
     }
     return socket;
}

void UserDatagramProtocolProvider::Disconnect(UserDatagramProtocolSocket *socket)
{
     for (uint16_t i = 0; i < numSockets && socket == 0; i++) {
          if (sockets[i] == socket) {
               sockets[i] = sockets[--numSockets];
               Kernel::MemoryManager::activeMemoryManager->free(socket);
               break;
          }
     }
}

void UserDatagramProtocolProvider::Send(UserDatagramProtocolSocket *socket, uint8_t *data, uint16_t size)
{
     uint16_t totalLength = size + sizeof(UserDatagramProtocolHeader);
     uint8_t *buffer = (uint8_t *)Kernel::MemoryManager::activeMemoryManager->malloc(totalLength);
     uint8_t *buffer2 = buffer + sizeof(UserDatagramProtocolHeader);
     UserDatagramProtocolHeader *msg = (UserDatagramProtocolHeader *)buffer;
     msg->srcPort = socket->localPort;
     msg->dstPort = socket->remotePort;
     msg->length = SWAP_ENDIAN_16(totalLength);
     for (int i = 0; i < size; i++) {
          buffer2[i] = data[i];
     }
     msg->checksum = 0;
     InternetProtocolHandler::Send(socket->remoteIP, buffer, totalLength);
     Kernel::MemoryManager::activeMemoryManager->free(buffer);
}

void UserDatagramProtocolProvider::Bind(UserDatagramProtocolSocket *socket,
     UserDatagramProtocolHandler *handler)
{
     socket->handler = handler;
}
}
}
