#include <net/udp.h>
#include <kernel/memory_manager.h>

namespace JLOS {
namespace Net {
user_datagram_protocol_handler::user_datagram_protocol_handler(){}

user_datagram_protocol_handler::~user_datagram_protocol_handler(){}

void user_datagram_protocol_handler::handle_user_datagram_protocol_message(
     user_datagram_protocol_socket *socket, uint8_t *m_data, uint16_t m_size){}

user_datagram_protocol_socket::user_datagram_protocol_socket(user_datagram_protocol_provider *backend)
: backend(backend), handler(0), m_listening(false){}

user_datagram_protocol_socket::~user_datagram_protocol_socket(){}
void user_datagram_protocol_socket::handle_user_datagram_protocol_message(uint8_t *m_data, uint16_t m_size)
{
     if (handler) {
          handler->handle_user_datagram_protocol_message(this, m_data, m_size);
     }
}

void user_datagram_protocol_socket::send(uint8_t *m_data, uint16_t m_size)
{
     backend->send(this, m_data ,m_size);
}

void user_datagram_protocol_socket::disconnect()
{
     backend->disconnect(this);
}

user_datagram_protocol_provider::user_datagram_protocol_provider(internet_protocol_provider *backend)
: internet_protocol_handler(backend, 0x11), m_num_sockets(0), m_free_port(1024)
{
     for (int i = 0; i < 65535; i++) {
          sockets[i] = 0;
     }
}

user_datagram_protocol_provider::~user_datagram_protocol_provider(){}

bool user_datagram_protocol_provider::on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
     uint8_t *internet_protocol_payload, uint32_t m_size)
{
     if (m_size < sizeof(user_datagram_protocol_header)) {
          return false;
     }
     user_datagram_protocol_header *msg = (user_datagram_protocol_header *)internet_protocol_payload;
     uint16_t m_local_port = msg->m_dst_port;
     uint16_t m_remote_port = msg->m_src_port;

     user_datagram_protocol_socket *socket = 0;
     for (uint16_t i = 0; i < m_num_sockets && socket == 0; i++) {
          if (sockets[i]->m_local_port == msg->m_dst_port && sockets[i]->m_local_ip == dstIP_BE
               && sockets[i]->m_listening) {
               socket = sockets[i];
               socket->m_listening = false;
               socket->m_remote_port = msg->m_src_port;
               socket->m_remote_ip = srcIP_BE;
          }
          else if (sockets[i]->m_local_port == msg->m_dst_port && sockets[i]->m_local_ip == dstIP_BE
               && sockets[i]->m_remote_port == msg->m_src_port && sockets[i]->m_remote_ip == srcIP_BE) {
               socket = sockets[i];
          }
     }
     if (socket) {
          socket->handle_user_datagram_protocol_message(internet_protocol_payload +
               sizeof(user_datagram_protocol_header), m_size - sizeof(user_datagram_protocol_header));
     }
     return false;
}

user_datagram_protocol_socket *user_datagram_protocol_provider::connect(uint32_t ip, uint16_t port)
{
     user_datagram_protocol_socket *socket =
          (user_datagram_protocol_socket *)Kernel::memory_manager::active_memory_manager->malloc(
               sizeof(user_datagram_protocol_socket));
     if (socket) {
          new (socket) user_datagram_protocol_socket(this);
          socket->m_remote_port = port;
          socket->m_remote_ip = ip;
          socket->m_local_port = m_free_port++;
          socket->m_local_ip = backend->get_ip_address();
          socket->m_remote_port = SWAP_ENDIAN_16(socket->m_remote_port);
          socket->m_local_port = SWAP_ENDIAN_16(socket->m_local_port);
          sockets[m_num_sockets++] = socket;
     }
     return socket;
}

user_datagram_protocol_socket *user_datagram_protocol_provider::listen(uint16_t port)
{
     user_datagram_protocol_socket *socket =
          (user_datagram_protocol_socket *)Kernel::memory_manager::active_memory_manager->malloc(
               sizeof(user_datagram_protocol_socket));
     if (socket) {
          new (socket) user_datagram_protocol_socket(this);
          socket->m_listening = true;
          socket->m_local_port = port;
          socket->m_local_ip = backend->get_ip_address();
          socket->m_local_port = SWAP_ENDIAN_16(socket->m_local_port);
          sockets[m_num_sockets++] = socket;
     }
     return socket;
}

void user_datagram_protocol_provider::disconnect(user_datagram_protocol_socket *socket)
{
     for (uint16_t i = 0; i < m_num_sockets && socket == 0; i++) {
          if (sockets[i] == socket) {
               sockets[i] = sockets[--m_num_sockets];
               Kernel::memory_manager::active_memory_manager->free(socket);
               break;
          }
     }
}

void user_datagram_protocol_provider::send(user_datagram_protocol_socket *socket, uint8_t *m_data, uint16_t m_size)
{
     uint16_t m_total_length = m_size + sizeof(user_datagram_protocol_header);
     uint8_t *buffer = (uint8_t *)Kernel::memory_manager::active_memory_manager->malloc(m_total_length);
     uint8_t *buffer2 = buffer + sizeof(user_datagram_protocol_header);
     user_datagram_protocol_header *msg = (user_datagram_protocol_header *)buffer;
     msg->m_src_port = socket->m_local_port;
     msg->m_dst_port = socket->m_remote_port;
     msg->m_length = SWAP_ENDIAN_16(m_total_length);
     for (int i = 0; i < m_size; i++) {
          buffer2[i] = m_data[i];
     }
     msg->m_checksum = 0;
     internet_protocol_handler::send(socket->m_remote_ip, buffer, m_total_length);
     Kernel::memory_manager::active_memory_manager->free(buffer);
}

void user_datagram_protocol_provider::bind(user_datagram_protocol_socket *socket,
     user_datagram_protocol_handler *handler)
{
     socket->handler = handler;
}
}
}
