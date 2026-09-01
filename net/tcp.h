#ifndef __JLOS_NET_TCP_H
#define __JLOS_NET_TCP_H

#include <net/ipv4.h>
#include <dsa/hash_chain.h>

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
    JLOS_TCP_FIN = 0x0001,
    JLOS_TCP_SYN = 0x0002,
    JLOS_TCP_RST = 0x0004,
    JLOS_TCP_PSH = 0x0008,
    JLOS_TCP_ACK = 0x0010,
    JLOS_TCP_URG = 0x0020,
    JLOS_TCP_ECE = 0x0040,
    JLOS_TCP_CWR = 0x0080,
    JLOS_TCP_NS  = 0x0100,
} jlos_tcp_flag_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t sequence_number;
    uint32_t acknowledgement_number;
    uint16_t data_offset_flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
    uint32_t options;
} __attribute__((packed)) jlos_tcp_header_t;

#define JLOS_TCP_GET_DATA_OFFSET(msg) (((((uint8_t*)&(msg)->data_offset_flags)[0]) >> 4) & 0x0F)
#define JLOS_TCP_GET_FLAGS(msg)       ((((uint8_t*)&(msg)->data_offset_flags)[1]) | ((((uint8_t*)&(msg)->data_offset_flags)[0] & 0x01) << 8))
#define JLOS_TCP_SET_DATA_OFFSET_FLAGS(msg, doff, flags) \
    do { \
        uint8_t *__p = (uint8_t*)&(msg)->data_offset_flags; \
        __p[0] = (((doff) & 0x0F) << 4) | (((flags) >> 8) & 0x01); \
        __p[1] = (flags) & 0xFF; \
    } while(0)

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t protocol;
    uint16_t total_length;
} __attribute__((packed)) jlos_tcp_pseudo_header_t;

typedef struct jlos_tcp_provider jlos_tcp_provider_t;
typedef struct jlos_tcp_socket jlos_tcp_socket_t;
typedef struct jlos_tcp_handler jlos_tcp_handler_t;

struct jlos_tcp_handler {
    bool (*handle_tcp_message)(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *data, uint16_t size);
};

struct jlos_tcp_socket {
    uint16_t                remote_port;
    uint32_t                remote_ip;
    uint16_t                local_port;
    uint32_t                local_ip;
    uint32_t                sequence_number;
    uint32_t                acknowledgement_number;
    jlos_tcp_provider_t     *backend;
    jlos_tcp_handler_t      *handler;
    jlos_tcp_socket_state_t state;
    jlos_hash_node_t        hash_node;
    bool (*handle_tcp_message)(struct jlos_tcp_socket* self, uint8_t *data, uint16_t size);
    void (*send)(struct jlos_tcp_socket* self, uint8_t *data, uint16_t size);
    void (*disconnect)(struct jlos_tcp_socket* self);
};

struct jlos_tcp_provider {
    jlos_internet_protocol_handler_t    base_handler;
    jlos_hash_chain_t                   sockets;
    uint16_t                            num_sockets;
    uint16_t                            free_port;
};

typedef struct {
    uint32_t ip;
    uint16_t port;
} jlos_tcp_key_t;

void jlos_tcp_handler_init(jlos_tcp_handler_t* self);
void jlos_tcp_handler_destroy(jlos_tcp_handler_t* self);
bool jlos_tcp_handler_handle_tcp_message(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *data, uint16_t size);

void jlos_tcp_socket_init(jlos_tcp_socket_t* self, jlos_tcp_provider_t *backend);
void jlos_tcp_socket_destroy(jlos_tcp_socket_t* self);
bool jlos_tcp_socket_handle_tcp_message(jlos_tcp_socket_t* self, uint8_t *data, uint16_t size);
void jlos_tcp_socket_send(jlos_tcp_socket_t* self, uint8_t *data, uint16_t size);
void jlos_tcp_socket_disconnect(jlos_tcp_socket_t* self);

void jlos_tcp_provider_init(jlos_tcp_provider_t* self, jlos_internet_protocol_provider_t *backend);
void jlos_tcp_provider_destroy(jlos_tcp_provider_t* self);
bool jlos_tcp_provider_on_internet_protocol_received(jlos_tcp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size);
jlos_tcp_socket_t *jlos_tcp_provider_connect(jlos_tcp_provider_t* self, uint32_t ip, uint16_t port);
jlos_tcp_socket_t *jlos_tcp_provider_listen(jlos_tcp_provider_t* self, uint16_t port);
void jlos_tcp_provider_disconnect(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket);
void jlos_tcp_provider_send(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, uint8_t *data, uint16_t size, uint16_t flags);
void jlos_tcp_provider_bind(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, jlos_tcp_handler_t *handler);

#endif