#ifndef __JLOS_NET_TCP_H
#define __JLOS_NET_TCP_H

#include <net/ipv4.h>

namespace JLOS {
namespace Net {
enum transmission_control_protocol_socket_state {
     CLOSED = 0,
     LISTEN,
     SYN_SENT,
     SYN_RECEIVED,
     ESTABLISHED,
     FIN_WAIT1,
     FIN_WAIT2,
     CLOSING,
     TIME_WAIT,
     CLOSE_WAIT,
};

enum transmission_control_protocol_flag {
     FIN = 1,
     SYN = 2,
     RST = 4,
     PSH = 8,
     ACK = 16,
     URG = 32,
     ECE = 64,
     CWR = 128,
     NS = 258,
};

struct transmission_control_protocol_header {
     uint16_t m_src_port;
     uint16_t m_dst_port;
     uint32_t m_sequence_number;
     uint32_t m_acknowledgement_number;
     uint8_t m_reserved{4};
     uint8_t header_size32{4};
     uint8_t m_flags;
     uint16_t m_window_size;
     uint16_t m_checksum;
     uint16_t m_urgent_ptr;
     uint32_t m_options;
} __attribute__((packed));

struct transmission_control_protocol_pseudo_header {
     uint32_t m_src_ip;
     uint32_t m_dst_ip;
     uint16_t m_protocol;
     uint16_t m_total_length;
} __attribute__((packed));

class transmission_control_protocol_socket;
class transmission_control_protocol_provider;

class transmission_control_protocol_handler {
public:
     transmission_control_protocol_handler();
     ~transmission_control_protocol_handler();

     virtual bool handle_transmission_control_protocol_message(transmission_control_protocol_socket *socket,
          uint8_t *m_data, uint16_t m_size);
};

class transmission_control_protocol_socket {
friend class transmission_control_protocol_provider;
protected:
     uint16_t m_remote_port;
     uint32_t m_remote_ip;
     uint16_t m_local_port;
     uint32_t m_local_ip;
     uint32_t m_sequence_number;
     uint32_t m_acknowledgement_number;
     transmission_control_protocol_provider *backend;
     transmission_control_protocol_handler *handler;
     transmission_control_protocol_socket_state m_state;

public:
     transmission_control_protocol_socket(transmission_control_protocol_provider *backend);
     ~transmission_control_protocol_socket();
     virtual bool handle_transmission_control_protocol_message(uint8_t *m_data, uint16_t m_size);
     virtual void send(uint8_t *m_data, uint16_t m_size);
     virtual void disconnect();
};

class transmission_control_protocol_provider : public internet_protocol_handler {
protected:
     transmission_control_protocol_socket *sockets[65535];
     uint16_t m_num_sockets;
     uint16_t m_free_port;

public:
     transmission_control_protocol_provider(internet_protocol_provider *backend);
     ~transmission_control_protocol_provider();

     virtual bool on_internet_protocol_received(uint32_t srcIP_BE, uint32_t dstIP_BE,
          uint8_t *internet_protocol_payload, uint32_t m_size);
     
     virtual transmission_control_protocol_socket *connect(uint32_t ip, uint16_t port);
     virtual transmission_control_protocol_socket *listen(uint16_t port);
     virtual void disconnect(transmission_control_protocol_socket *socket);
     virtual void send(transmission_control_protocol_socket *socket, uint8_t *m_data, uint16_t m_size,
          uint16_t m_flags = 0);
     virtual void bind(transmission_control_protocol_socket *socket, transmission_control_protocol_handler *handler);
};
class transmission_control_protocol {

};
}
}
#endif
