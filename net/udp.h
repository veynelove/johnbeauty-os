#ifndef __JLOS_NET_UDP_H
#define __JLOS_NET_UDP_H

#include <net/ipv4.h>
#include <dsa/hash_chain.h>

typedef struct jlos_udp_socket jlos_udp_socket_t;
typedef struct jlos_udp_provider jlos_udp_provider_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) jlos_udp_header_t;

typedef struct jlos_udp_handler jlos_udp_handler_t;

struct jlos_udp_handler {
    void (*handle_udp_message)(jlos_udp_handler_t* self, jlos_udp_socket_t* socket, uint8_t *data, uint16_t size);
};

struct jlos_udp_socket {
    uint16_t                remote_port;
    uint32_t                remote_ip;
    uint16_t                local_port;
    uint32_t                local_ip;
    jlos_udp_provider_t     *backend;
    jlos_udp_handler_t      *handler;
    jlos_hash_node_t        hash_node;
    bool                    listening;
    void (*handle_udp_message)(struct jlos_udp_socket* self, uint8_t *data, uint16_t size);
    void (*send)(struct jlos_udp_socket* self, uint8_t *data, uint16_t size);
    void (*disconnect)(struct jlos_udp_socket* self);
};

struct jlos_udp_provider {
    jlos_internet_protocol_handler_t    base_handler;
    jlos_hash_chain_t                   sockets;
    uint16_t                            num_sockets;
    uint16_t                            free_port;
};

typedef struct {
    uint32_t ip;
    uint16_t port;
} jlos_udp_key_t;

void jlos_udp_handler_init(jlos_udp_handler_t* self);
void jlos_udp_handler_destroy(jlos_udp_handler_t* self);
void jlos_udp_handler_handle_udp_message(jlos_udp_handler_t* self, jlos_udp_socket_t* socket, uint8_t *data, uint16_t size);

void jlos_udp_socket_init(jlos_udp_socket_t* self, jlos_udp_provider_t *backend);
void jlos_udp_socket_destroy(jlos_udp_socket_t* self);
void jlos_udp_socket_handle_udp_message(jlos_udp_socket_t* self, uint8_t *data, uint16_t size);
void jlos_udp_socket_send(jlos_udp_socket_t* self, uint8_t *data, uint16_t size);
void jlos_udp_socket_disconnect(jlos_udp_socket_t* self);

void jlos_udp_provider_init(jlos_udp_provider_t* self, jlos_internet_protocol_provider_t *backend);
void jlos_udp_provider_destroy(jlos_udp_provider_t* self);
bool jlos_udp_provider_on_internet_protocol_received(jlos_udp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size);
jlos_udp_socket_t *jlos_udp_provider_connect(jlos_udp_provider_t* self, uint32_t ip, uint16_t port);
jlos_udp_socket_t *jlos_udp_provider_listen(jlos_udp_provider_t* self, uint16_t port);
void jlos_udp_provider_disconnect(jlos_udp_provider_t* self, jlos_udp_socket_t *socket);
void jlos_udp_provider_send(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, uint8_t *data, uint16_t size);
void jlos_udp_provider_bind(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, jlos_udp_handler_t *handler);

#endif