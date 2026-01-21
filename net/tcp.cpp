#include <net/tcp.h>
#include <kernel/memory_manager.h>

namespace JLOS {
namespace Net {
transmission_control_protocol_handler::transmission_control_protocol_handler(){}

transmission_control_protocol_handler::~transmission_control_protocol_handler(){}

bool transmission_control_protocol_handler::handle_transmission_control_protocol_message(
     transmission_control_protocol_socket *socket, uint8_t *m_data, uint16_t m_size)
{
     return true;
}

transmission_control_protocol_socket::transmission_control_protocol_socket(transmission_control_protocol_provider *backend)
: backend(backend), handler(0), m_state(CLOSED){}

transmission_control_protocol_socket::~transmission_control_protocol_socket(){}
bool transmission_control_protocol_socket::handle_transmission_control_protocol_message(uint8_t *m_data, uint16_t m_size)
{
     if (handler) {
          return handler->handle_transmission_control_protocol_message(this, m_data, m_size);
     }
     return false;
}

void transmission_control_protocol_socket::send(uint8_t *m_data, uint16_t m_size)
{
     while (m_state != ESTABLISHED) {}
     backend->send(this, m_data ,m_size, (PSH | ACK));
}

void transmission_control_protocol_socket::disconnect()
{
     backend->disconnect(this);
}

transmission_control_protocol_provider::transmission_control_protocol_provider(internet_protocol_provider *backend)
: internet_protocol_handler(backend, 0x06), m_num_sockets(0), m_free_port(1024)
{
     for (int i = 0; i < 65535; i++) {
          sockets[i] = 0;
     }
}

transmission_control_protocol_provider::~transmission_control_protocol_provider(){}

bool transmission_control_protocol_provider::on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internet_protocol_payload, uint32_t m_size)
{
     if (m_size < 20) {
          return false;
     }
     transmission_control_protocol_header *msg = (transmission_control_protocol_header *)internet_protocol_payload;
     uint16_t m_local_port = msg->m_dst_port;
     uint16_t m_remote_port = msg->m_src_port;

     transmission_control_protocol_socket *socket = 0;
     for (uint16_t i = 0; i < m_num_sockets && socket == 0; i++) {
          if (sockets[i]->m_local_port == msg->m_dst_port && sockets[i]->m_local_ip == dstIP_BE
               && sockets[i]->m_state == LISTEN && ((msg->m_flags) & (SYN | ACK) == SYN)) {
               socket = sockets[i];
          }
          else if (sockets[i]->m_local_port == msg->m_dst_port && sockets[i]->m_local_ip == dstIP_BE
               && sockets[i]->m_remote_port == msg->m_src_port && sockets[i]->m_remote_ip == srcIP_BE) {
               socket = sockets[i];
          }
     }

     bool reset = false;
     if (socket && msg->m_flags && RST) {
          socket->m_state = CLOSED;
     }
     if (socket && socket->m_state != CLOSED) {
          switch ((msg->m_flags) & (SYN | ACK | FIN)) {
               case SYN :
                    if (socket->m_state == LISTEN) {
                         socket->m_state = SYN_RECEIVED;
                         socket->m_remote_port = msg->m_src_port;
                         socket->m_remote_ip = srcIP_BE;
                         socket->m_acknowledgement_number = SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
                         socket->m_sequence_number = 0xbeefcafe;
                         send(socket, 0, 0, (SYN | ACK));
                         socket->m_sequence_number++;
                    } else {
                         reset = true;
                    }
                    break;
               case (SYN | ACK) :
                    if (socket->m_state == SYN_SENT) {
                         socket->m_state = ESTABLISHED;
                         socket->m_acknowledgement_number = SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
                         socket->m_sequence_number++;
                         send(socket, 0, 0, ACK);
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
                    switch (socket->m_state) {
                         case ESTABLISHED :
                              socket->m_state = CLOSE_WAIT;
                              socket->m_acknowledgement_number++;
                              send(socket, 0, 0, ACK);
                              send(socket, 0, 0, (FIN | ACK));
                              break;
                         case CLOSE_WAIT :
                              socket->m_state = CLOSED;
                              break;
                         case (FIN_WAIT1 | FIN_WAIT2) :
                              socket->m_state = CLOSED;
                              socket->m_acknowledgement_number++;
                              send(socket, 0, 0, ACK);
                              break;
                         default :
                              reset = true;
                    }
                    break;
               case ACK :
                    switch (socket->m_state) {
                         case SYN_RECEIVED :
                              socket->m_state = ESTABLISHED;
                              return false;
                         case FIN_WAIT1 :
                              socket->m_state = FIN_WAIT2;
                              return false;
                         case CLOSE_WAIT :
                              socket->m_state = CLOSED;
                    }
                    break;
               default :
                    if (SWAP_ENDIAN_32(msg->m_sequence_number) == socket->m_acknowledgement_number) {
                         reset = !socket->handle_transmission_control_protocol_message((internet_protocol_payload +
                              msg->header_size32 * 4), (m_size - msg->header_size32 * 4));
                         if (!reset) {
                              socket->m_acknowledgement_number += (m_size - msg->header_size32 * 4);
                              send(socket, 0, 0, ACK);
                         }
                    } else {
                         // m_data in wrong order
                         reset = true;
                    }
          }
     }
     if (reset) {
          if (socket) {
               send(socket, 0, 0, RST);
          } else {
               transmission_control_protocol_socket socket1(this);
               socket1.m_remote_port = msg->m_src_port;
               socket1.m_remote_ip = srcIP_BE;
               socket1.m_local_port = msg->m_dst_port;
               socket1.m_local_ip = dstIP_BE;
               socket1.m_sequence_number = SWAP_ENDIAN_32(msg->m_acknowledgement_number);
               socket1.m_acknowledgement_number = SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
               send(&socket1, 0, 0, RST);
               return true;
          }
     }
     if (socket && socket->m_state == CLOSED) {
          for (uint16_t i = 0; i < m_num_sockets && socket == 0; i++) {
               if (sockets[i] == socket) {
                    sockets[i] = sockets[--m_num_sockets];
                    Kernel::memory_manager::active_memory_manager->free(socket);
                    break;
               }
          }
     }
     return false;
}

void transmission_control_protocol_provider::send(transmission_control_protocol_socket *socket,
     uint8_t *m_data, uint16_t m_size, uint16_t m_flags)
{
     uint16_t m_total_length = m_size + sizeof(transmission_control_protocol_header);
     uint16_t length_incl_p_hdr = m_total_length + sizeof(transmission_control_protocol_pseudo_header);

     uint8_t *buffer = (uint8_t *)Kernel::memory_manager::active_memory_manager->malloc(length_incl_p_hdr);
     
     transmission_control_protocol_pseudo_header *phdr = (transmission_control_protocol_pseudo_header *)buffer;
     transmission_control_protocol_header *msg = (transmission_control_protocol_header *)(buffer +
          sizeof(transmission_control_protocol_pseudo_header));
     
     uint8_t *buffer2 = buffer + sizeof(transmission_control_protocol_header) +
          sizeof(transmission_control_protocol_pseudo_header);

     msg->header_size32 = sizeof(transmission_control_protocol_handler) / 4;
     msg->m_src_port = socket->m_local_port;
     msg->m_dst_port = socket->m_remote_port;
     msg->m_acknowledgement_number = SWAP_ENDIAN_32(socket->m_acknowledgement_number);
     msg->m_sequence_number = SWAP_ENDIAN_32(socket->m_sequence_number);
     msg->m_reserved = 0;
     msg->m_flags = m_flags;
     msg->m_window_size = 0xFFFF;
     msg->m_urgent_ptr = 0;
     msg->m_options = ((m_flags & SYN) != 0) ? 0xB4050402 : 0;
     
     socket->m_sequence_number += m_size;

     for (int i = 0; i < m_size; i++) {
          buffer2[i] = m_data[i];
     }
     phdr->m_src_ip = socket->m_local_ip;
     phdr->m_dst_ip = socket->m_remote_ip;
     phdr->m_protocol = 0x0600;
     phdr->m_total_length = SWAP_ENDIAN_16(m_total_length);
     msg->m_checksum = 0;
     msg->m_checksum = internet_protocol_provider::m_check_sum((uint16_t *)buffer, length_incl_p_hdr);
     internet_protocol_handler::send(socket->m_remote_ip, (uint8_t *)msg, m_total_length);
     Kernel::memory_manager::active_memory_manager->free(buffer);
}


transmission_control_protocol_socket *transmission_control_protocol_provider::connect(uint32_t ip, uint16_t port)
{
     transmission_control_protocol_socket *socket =
          (transmission_control_protocol_socket *)Kernel::memory_manager::active_memory_manager->malloc(
               sizeof(transmission_control_protocol_socket));
     if (socket) {
          new (socket) transmission_control_protocol_socket(this);
          socket->m_remote_port = port;
          socket->m_remote_ip = ip;
          socket->m_local_port = m_free_port++;
          socket->m_local_ip = backend->get_ip_address();
          socket->m_remote_port = SWAP_ENDIAN_16(socket->m_remote_port);
          socket->m_local_port = SWAP_ENDIAN_16(socket->m_local_port);
          sockets[m_num_sockets++] = socket;
          socket->m_state = SYN_SENT;
          socket->m_sequence_number = 0xbeefcafe;
          send(socket, 0, 0, SYN);
     }
     return socket;
}

void transmission_control_protocol_provider::disconnect(transmission_control_protocol_socket *socket)
{
     socket->m_state = FIN_WAIT1;
     send(socket, 0, 0, FIN + ACK);
     socket->m_sequence_number++;
}

transmission_control_protocol_socket *transmission_control_protocol_provider::listen(uint16_t port)
{
     transmission_control_protocol_socket *socket =
          (transmission_control_protocol_socket *)Kernel::memory_manager::active_memory_manager->malloc(
               sizeof(transmission_control_protocol_socket));
     if (socket) {
          new (socket) transmission_control_protocol_socket(this);
          socket->m_state = LISTEN;
          socket->m_local_ip = backend->get_ip_address();
          socket->m_local_port = SWAP_ENDIAN_16(port);
          sockets[m_num_sockets++] = socket;
     }
     return socket;
}

void transmission_control_protocol_provider::bind(transmission_control_protocol_socket *socket,
     transmission_control_protocol_handler *handler)
{
     socket->handler = handler;
}
}
}
