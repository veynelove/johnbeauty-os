#ifndef __JLOS_NET_TCP_H
#define __JLOS_NET_TCP_H

#include <net/ipv4.h>

typedef enum {
    JLOS_TCP_CLOSED = 0,
    JLOS_TCP_LISTEN,
    JLOS_TCP_SYN_SENT,
    JLOS_TCP_SYN_RECEIVED,
    JLOS_TCP_ESTABLISHED,
    JLOS_TCP_FIN_WAIT1,
    JLOS_TCP_FIN_WAIT2,
    JLOS_TCP_CLOSING,
    JLOS_TCP_TIME_WAIT,
    JLOS_TCP_CLOSE_WAIT,
} jlos_tcp_socket_state_t;

typedef enum {
    JLOS_TCP_FIN = 1,
    JLOS_TCP_SYN = 2,
    JLOS_TCP_RST = 4,
    JLOS_TCP_PSH = 8,
    JLOS_TCP_ACK = 16,
    JLOS_TCP_URG = 32,
    JLOS_TCP_ECE = 64,
    JLOS_TCP_CWR = 128,
    JLOS_TCP_NS = 258,
} jlos_tcp_flag_t;

typedef struct {
    uint16_t m_src_port;
    uint16_t m_dst_port;
    uint32_t m_sequence_number;
    uint32_t m_acknowledgement_number;
    uint8_t m_reserved;
    uint8_t header_size32;
    uint8_t m_flags;
    uint16_t m_window_size;
    uint16_t m_checksum;
    uint16_t m_urgent_ptr;
    uint32_t m_options;
} __attribute__((packed)) jlos_tcp_header_t;

typedef struct {
    uint32_t m_src_ip;
    uint32_t m_dst_ip;
    uint16_t m_protocol;
    uint16_t m_total_length;
} __attribute__((packed)) jlos_tcp_pseudo_header_t;

typedef struct jlos_tcp_provider jlos_tcp_provider_t;
typedef struct jlos_tcp_socket jlos_tcp_socket_t;
typedef struct jlos_tcp_handler jlos_tcp_handler_t;

struct jlos_tcp_handler {
    bool (*handle_tcp_message)(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *m_data, uint16_t m_size);
};

struct jlos_tcp_socket {
    uint16_t m_remote_port;
    uint32_t m_remote_ip;
    uint16_t m_local_port;
    uint32_t m_local_ip;
    uint32_t m_sequence_number;
    uint32_t m_acknowledgement_number;
    jlos_tcp_provider_t *backend;
    jlos_tcp_handler_t *handler;
    jlos_tcp_socket_state_t m_state;
    bool (*handle_tcp_message)(struct jlos_tcp_socket* self, uint8_t *m_data, uint16_t m_size);
    void (*send)(struct jlos_tcp_socket* self, uint8_t *m_data, uint16_t m_size);
    void (*disconnect)(struct jlos_tcp_socket* self);
};

struct jlos_tcp_provider {
    jlos_internet_protocol_handler_t base_handler;
    jlos_tcp_socket_t *sockets[65535];
    uint16_t m_num_sockets;
    uint16_t m_free_port;
};

void jlos_tcp_handler_init(jlos_tcp_handler_t* self);
void jlos_tcp_handler_destroy(jlos_tcp_handler_t* self);
bool jlos_tcp_handler_handle_tcp_message(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *m_data, uint16_t m_size);

void jlos_tcp_socket_init(jlos_tcp_socket_t* self, jlos_tcp_provider_t *backend);
void jlos_tcp_socket_destroy(jlos_tcp_socket_t* self);
bool jlos_tcp_socket_handle_tcp_message(jlos_tcp_socket_t* self, uint8_t *m_data, uint16_t m_size);
void jlos_tcp_socket_send(jlos_tcp_socket_t* self, uint8_t *m_data, uint16_t m_size);
void jlos_tcp_socket_disconnect(jlos_tcp_socket_t* self);

void jlos_tcp_provider_init(jlos_tcp_provider_t* self, jlos_internet_protocol_provider_t *backend);
void jlos_tcp_provider_destroy(jlos_tcp_provider_t* self);
bool jlos_tcp_provider_on_internet_protocol_received(jlos_tcp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size);
jlos_tcp_socket_t *jlos_tcp_provider_connect(jlos_tcp_provider_t* self, uint32_t ip, uint16_t port);
jlos_tcp_socket_t *jlos_tcp_provider_listen(jlos_tcp_provider_t* self, uint16_t port);
void jlos_tcp_provider_disconnect(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket);
void jlos_tcp_provider_send(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, uint8_t *m_data, uint16_t m_size, uint16_t m_flags);
void jlos_tcp_provider_bind(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, jlos_tcp_handler_t *handler);

#endif