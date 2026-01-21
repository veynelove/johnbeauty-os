#include <net/icmp.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printf_hex(uint8_t);
}

namespace JLOS {
namespace Net {
internet_control_message_protocol::internet_control_message_protocol(internet_protocol_provider *backend)
: internet_protocol_handler(backend, 0x01){}

internet_control_message_protocol::~internet_control_message_protocol(){}

bool internet_control_message_protocol::on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internet_protocol_payload, uint32_t m_size)
{
     if (m_size < sizeof(internet_control_message_protocol_message)) {
          return false;
     }
     internet_control_message_protocol_message *msg =
          (internet_control_message_protocol_message *)internet_protocol_payload;
     switch (msg->m_type) {
          case 0 :
               Kernel::printf("ping response from ");
               Kernel::printf_hex(srcIP_BE & 0xFF);
               Kernel::printf(".");
               Kernel::printf_hex((srcIP_BE >> 8) & 0xFF);
               Kernel::printf(".");
               Kernel::printf_hex((srcIP_BE >> 16) & 0xFF);
               Kernel::printf(".");
               Kernel::printf_hex((srcIP_BE >> 24) & 0xFF);
               Kernel::printf("\n");
               break;
          case 8 :
               msg->m_type = 0;
               msg->m_check_sum = 0;
               msg->m_check_sum = internet_protocol_provider::m_check_sum((uint16_t *)&msg,
                    sizeof(internet_control_message_protocol_message));
               return true;
     }
     return false;
}

void internet_control_message_protocol::request_echo_reply(uint32_t ip_be)
{
     internet_control_message_protocol_message icmp;
     icmp.m_type = 8; // ping
     icmp.m_code = 0;
     icmp.m_data = 0x3713; // 1337
     icmp.m_check_sum = 0;
     icmp.m_check_sum = internet_protocol_provider::m_check_sum((uint16_t *)&icmp,
          sizeof(internet_control_message_protocol_message));
     internet_protocol_handler::send(ip_be, (uint8_t *)&icmp,
          sizeof(internet_control_message_protocol_message));
     
}
}
}
