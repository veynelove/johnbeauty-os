/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_NET_SOCKET_H
#define _JLOS_NET_SOCKET_H

#include <common/types.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <dsa/list.h>
#include <hal/spinlock.h>
#include <kernel/sync.h>

#define JLOS_SOCKET_DOMAIN_INET     2
#define JLOS_SOCKET_STREAM          1
#define JLOS_SOCKET_DGRAM           2

#define JLOS_SOCKET_RECVBUF_SIZE    (64 * 1024)

typedef struct {
    uint32_t            domain;
    uint32_t            type;
    uint32_t            protocol;
    uint32_t            local_ip;
    uint16_t            local_port;
union {
jlos_tcp_socket_t *tcp;
jlos_udp_socket_t *udp;
}                       proto;

    jlos_udp_handler_t  udp_handler;
    jlos_list_head_t    recv_queue;
    jlos_spinlock_t     recv_lock;
    jlos_semaphore_t    recv_wait;
    uint32_t            recv_queue_bytes;
    bool                bound;
    bool                listening;
    bool                closed;
} jlos_socket_t;

typedef struct {
    uint16_t    family;
    uint16_t    port;
    uint32_t    addr;
    uint8_t     zero[8];
} __attribute__((packed)) jlos_sockaddr_in_t;

jlos_socket_t *jlos_socket_create(uint32_t domain, uint32_t type, uint32_t protocol);
void jlos_socket_destroy(jlos_socket_t *self);
int32_t jlos_socket_bind(jlos_socket_t *self, uint32_t ip, uint16_t port);
int32_t jlos_socket_listen(jlos_socket_t *self, uint32_t backlog);
jlos_socket_t *jlos_socket_accept(jlos_socket_t *self);
int32_t jlos_socket_connect(jlos_socket_t *self, uint32_t ip, uint16_t port);
int32_t jlos_socket_send(jlos_socket_t *self, const void *buf, uint32_t len);
int32_t jlos_socket_recv(jlos_socket_t *self, void *buf, uint32_t len);
void jlos_socket_close(jlos_socket_t *self);

void jlos_socket_getpeer_name(jlos_socket_t *self, uint32_t *ip, uint16_t *port);

#endif
