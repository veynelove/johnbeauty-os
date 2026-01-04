#include <net/icmp.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printfHex(uint8_t);
}

namespace JLOS {
namespace Net {
InternetControlMessageProtocol::InternetControlMessageProtocol(InternetProtocolProvider *backend)
: InternetProtocolHandler(backend, 0x01){}

InternetControlMessageProtocol::~InternetControlMessageProtocol(){}

bool InternetControlMessageProtocol::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internetProtocolPayload, uint32_t size)
{
     if (size < sizeof(InternetControlMessageProtocolMessage)) {
          return false;
     }
     InternetControlMessageProtocolMessage *msg =
          (InternetControlMessageProtocolMessage *)internetProtocolPayload;
     switch (msg->type) {
          case 0 :
               Kernel::printf("ping response from ");
               Kernel::printfHex(srcIP_BE & 0xFF);
               Kernel::printf(".");
               Kernel::printfHex((srcIP_BE >> 8) & 0xFF);
               Kernel::printf(".");
               Kernel::printfHex((srcIP_BE >> 16) & 0xFF);
               Kernel::printf(".");
               Kernel::printfHex((srcIP_BE >> 24) & 0xFF);
               Kernel::printf("\n");
               break;
          case 8 :
               msg->type = 0;
               msg->checkSum = 0;
               msg->checkSum = InternetProtocolProvider::CheckSum((uint16_t *)&msg,
                    sizeof(InternetControlMessageProtocolMessage));
               return true;
     }
     return false;
}

void InternetControlMessageProtocol::RequestEchoReply(uint32_t ip_be)
{
     InternetControlMessageProtocolMessage icmp;
     icmp.type = 8; // ping
     icmp.code = 0;
     icmp.data = 0x3713; // 1337
     icmp.checkSum = 0;
     icmp.checkSum = InternetProtocolProvider::CheckSum((uint16_t *)&icmp,
          sizeof(InternetControlMessageProtocolMessage));
     InternetProtocolHandler::Send(ip_be, (uint8_t *)&icmp,
          sizeof(InternetControlMessageProtocolMessage));
     
}
}
}
