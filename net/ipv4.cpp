#include <net/ipv4.h>
namespace JLOS {
namespace Net {
InternetProtocolHandler::InternetProtocolHandler(InternetProtocolProvider *backend, uint8_t protocol)
: backend(backend), ip_protocol(protocol)
{
     backend->handlers[protocol] = this;
}

InternetProtocolHandler::~InternetProtocolHandler()
{
     if (backend->handlers[ip_protocol] == this) {
          backend->handlers[ip_protocol] = nullptr;
     }
}

bool InternetProtocolHandler::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internetProtocolPayload, uint32_t size)
{
     return false;
}

void InternetProtocolHandler::Send(uint32_t dstIP_BE, uint8_t *internetProtocolPayload,
     uint8_t *buffer, uint32_t size)
{
     backend->Send(dstIP_BE, ip_protocol, internetProtocolPayload, size);
}

InternetProtocolProvider::InternetProtocolProvider(EtherFrameProvider *backend,
     AddressResolutionProtocol *arp, uint32_t gatewayIP, uint32_t subnetMask)
: EtherFrameHandler(backend, 0x800), arp(arp), gatewayIP(gatewayIP), subnetMask(subnetMask)
{
     for (int i = 0; i < 255; i++) {
          handlers[i] = nullptr;
     }
}

InternetProtocolProvider::~InternetProtocolProvider(){}

bool InternetProtocolProvider::OnEtherFrameReceived(uint8_t *etherframePayload, uint32_t size)
{
     if (size < sizeof(InternetProtocolV4Message)) {
          return false;
     }
     InternetProtocolV4Message *ipMessage = (InternetProtocolV4Message *)etherframePayload;
     bool sendBack = false;
     if (ipMessage->dstIP == backend->GetIPAddress()) {
          int length = ipMessage->totalLength;
          if (length > size) {
               length = size; // defend the hard bleed attack
          }
          if (handlers[ipMessage->protocol]) {
               sendBack = handlers[ipMessage->protocol]->OnInternetProtocolReceived(
                   ipMessage->srcIP, ipMessage->dstIP, 
                   etherframePayload + 4 * ipMessage->headerLength, length - 4 * ipMessage->headerLength);
          }
     }
     if (sendBack) {
          uint32_t temp = ipMessage->dstIP;
          ipMessage->dstIP = ipMessage->srcIP;
          ipMessage->srcIP = temp;

          ipMessage->timeToLive = 0x40;
          ipMessage->checksum = 0;
          ipMessage->checksum = CheckSum((uint16_t *)ipMessage, 4 * ipMessage->headerLength);
     }
     return sendBack;
}

void InternetProtocolProvider::Send(uint32_t dstIP_BE, uint8_t protocol, uint8_t *data, uint32_t size)
{
     uint8_t *buffer = 
          (uint8_t *)Kernel::MemoryManager::activeMemoryManager->malloc(sizeof(InternetProtocolV4Message) + size);
     InternetProtocolV4Message *message = (InternetProtocolV4Message *)buffer;
     message->version = 4;
     message->headerLength = sizeof(InternetProtocolV4Message)/4;
     message->tos = 0;
     message->totalLength = size + sizeof(InternetProtocolV4Message);
     message->totalLength = SWAP_ENDIAN_16(message->totalLength);
     message->ident = 0x0100;
     message->flagsAndOffset = 0x0040;
     message->timeToLive = 0x40;
     message->protocol = protocol;
     
     message->dstIP = dstIP_BE;
     message->srcIP = backend->GetIPAddress();

     message->checksum = 0;
     message->checksum = CheckSum((uint16_t *)message, sizeof(InternetProtocolV4Message));

     uint8_t *dataBuffer = buffer + sizeof(InternetProtocolV4Message);
     for (int i = 0; i < size; i++) {
          dataBuffer[i] = data[i];
     }
     uint32_t route = dstIP_BE;
     if ((dstIP_BE & subnetMask) != (message->srcIP & subnetMask)) {
          route = gatewayIP;
     }
     backend->Send(arp->Resolve(route),
          this->etherType_BE, buffer, sizeof(InternetProtocolV4Message) + size);
     Kernel::MemoryManager::activeMemoryManager->free(buffer);
}

uint16_t InternetProtocolProvider::CheckSum(uint16_t *data, uint32_t lengthInBytes)
{
     uint32_t temp = 0;
     for (int i = 0; i < lengthInBytes/2; i++) {
          temp += SWAP_ENDIAN_16(data[i]);
     }
     if (lengthInBytes % 2) {
          temp += (uint16_t)(((char *)data)[lengthInBytes - 1]) << 8;
     }
     while (temp & 0xFFFF0000) {
          temp = (temp & 0xFFFF) + (temp >> 16);
     }
     return SWAP_ENDIAN_16(temp);
}
}
}
