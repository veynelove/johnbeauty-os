#ifndef __JLOS_NET_UDP_H
#define __JLOS_NET_UDP_H

#include <net/ipv4.h>

namespace JLOS {
namespace Net {
struct user_datagram_protocol_header {
     uint16_t m_src_port;
     uint16_t m_dst_port;
     uint16_t m_length;
     uint16_t m_checksum;
} __attribute__((packed));

class user_datagram_protocol_socket;
class user_datagram_protocol_provider;

class user_datagram_protocol_handler {
public:
     user_datagram_protocol_handler();
     ~user_datagram_protocol_handler();

     virtual void handle_user_datagram_protocol_message(user_datagram_protocol_socket *socket,
          uint8_t *m_data, uint16_t m_size);
};

class user_datagram_protocol_socket {
friend class user_datagram_protocol_provider;
protected:
     uint16_t m_remote_port;
     uint32_t m_remote_ip;
     uint16_t m_local_port;
     uint32_t m_local_ip;
     user_datagram_protocol_provider *backend;
     user_datagram_protocol_handler *handler;
     bool m_listening;

public:
     user_datagram_protocol_socket(user_datagram_protocol_provider *backend);
     ~user_datagram_protocol_socket();
     virtual void handle_user_datagram_protocol_message(uint8_t *m_data, uint16_t m_size);
     virtual void send(uint8_t *m_data, uint16_t m_size);
     virtual void disconnect();
};

class user_datagram_protocol_provider : public internet_protocol_handler {
protected:
     user_datagram_protocol_socket *sockets[65535];
     uint16_t m_num_sockets;
     uint16_t m_free_port;

public:
     user_datagram_protocol_provider(internet_protocol_provider *backend);
     ~user_datagram_protocol_provider();

     virtual bool on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
          uint8_t *internet_protocol_payload, uint32_t m_size);
     
     virtual user_datagram_protocol_socket *connect(uint32_t ip, uint16_t port);
     virtual user_datagram_protocol_socket *listen(uint16_t port);
     virtual void disconnect(user_datagram_protocol_socket *socket);
     virtual void send(user_datagram_protocol_socket *socket, uint8_t *m_data, uint16_t m_size);
     virtual void bind(user_datagram_protocol_socket *socket, user_datagram_protocol_handler *handler);
};
}
}
#endif
