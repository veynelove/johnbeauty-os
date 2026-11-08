/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_NET_TCP_H
#define _JLOS_NET_TCP_H

#include <net/ipv4.h>
#include <net/skbuff.h>
#include <dsa/hash_chain.h>
#include <kernel/sync.h>
#include <kernel/hrtimer.h>

#define JLOS_TCP_MSS                        1460
#define JLOS_TCP_MSS_OPTIONS_KIND           2
#define JLOS_TCP_MSS_OPTION_LEN             4
#define JLOS_TCP_MIN_DATA_OFFSET            5
#define JLOS_TCP_DATA_OFFSET_SYN            (JLOS_TCP_MIN_DATA_OFFSET + 1)
#define JLOS_TCP_HEADER_LEN_SYN             (JLOS_TCP_DATA_OFFSET_SYN * 4)
#define JLOS_TCP_WINDOW_MAX                 0xFFFF
#define JLOS_TCP_SEQ_MAX                    0xFFFFFFFF
#define JLOS_TCP_TIMEWAIT_TIMEOUT_NS        ((uint64_t)60 * 1000 * 1000 * 1000)
#define JLOS_TCP_CLOSED_RELEASE_NS          ((uint64_t)1 * 1000 * 1000 * 1000)
#define JLOS_TCP_RCVBUF_SIZE                (64 * 1024)

#define JLOS_TCP_RETRANS_TIMEOUT_INIT_MS    1000
#define JLOS_TCP_RETRANS_TIMEOUT_MAX_MS     60000
#define JLOS_TCP_SYNACK_RETRANS_MAX         3

#define JLOS_TCP_ACCEPT_BACKLOG_MAX         16

#define JLOS_TCP_GET_DATA_OFFSET(msg)       (((((uint8_t*)&(msg)->data_offset_flags)[0]) >> 4) & 0x0F)
#define JLOS_TCP_GET_FLAGS(msg)             ((((uint8_t*)&(msg)->data_offset_flags)[1]) | ((((uint8_t*)&(msg)->data_offset_flags)[0] & 0x01) << 8))

#define JLOS_TCP_SET_DATA_OFFSET_FLAGS(msg, doff, flags) \
    do { \
        uint8_t *__p = (uint8_t*)&(msg)->data_offset_flags; \
        __p[0] = (((doff) & 0x0F) << 4) | (((flags) >> 8) & 0x01); \
        __p[1] = (flags) & 0xFF; \
    } while(0)

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
    JLOS_TCP_LAST_ACK,
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
} __attribute__((packed)) jlos_tcp_header_t;

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t protocol;
    uint16_t total_length;
} __attribute__((packed)) jlos_tcp_pseudo_header_t;

typedef struct jlos_tcp_provider jlos_tcp_provider_t;
typedef struct jlos_tcp_socket jlos_tcp_socket_t;

struct jlos_tcp_socket {
    uint16_t                remote_port;
    uint32_t                remote_ip;
    uint16_t                local_port;
    uint32_t                local_ip;
    uint32_t                sequence_number;
    uint32_t                acknowledgement_number;

    jlos_tcp_provider_t     *backend;
    jlos_tcp_socket_state_t state;
    jlos_cond_t             state_cond;
    jlos_mutex_t            state_mutex;
    jlos_semaphore_t        state_event;
    jlos_hrtimer_t          release_timer;
    jlos_hash_node_t        hash_node;
    jlos_list_head_t        accept_node;
    jlos_tcp_socket_t       *listener;
    jlos_list_head_t        receive_queue;
    jlos_spinlock_t         receive_lock;
    jlos_semaphore_t        receive_wait;
    uint32_t                receive_bytes;
    jlos_list_head_t        retrans_queue;
    jlos_spinlock_t         retrans_lock;
    jlos_hrtimer_t          retrans_timer;
    uint32_t                retrans_timeout_ms;
    uint32_t                synack_retrans_count;
    void (*send)(struct jlos_tcp_socket* self, uint8_t *data, uint16_t size);
    void (*disconnect)(struct jlos_tcp_socket* self);
};

struct jlos_tcp_provider {
    jlos_internet_protocol_handler_t    base_handler;
    jlos_hash_chain_t                   sockets;
    jlos_hash_chain_t                   listeners;
    jlos_list_head_t                    accept_queue;
    jlos_spinlock_t                     accept_lock;
    uint16_t                            num_sockets;
    uint16_t                            free_port;
};

typedef struct {
    uint32_t ip;
    uint16_t port;
} jlos_tcp_key_t;

typedef struct {
    jlos_list_head_t    list;
    jlos_net_sk_buff_t  *skb;
    uint32_t            len;
    uint32_t            seq;
    uint32_t            end_seq;
    uint16_t            flags;
} jlos_tcp_retrans_t;

void jlos_tcp_socket_init(jlos_tcp_socket_t* self, jlos_tcp_provider_t *backend);
void jlos_tcp_socket_destroy(jlos_tcp_socket_t* self);
void jlos_tcp_socket_send(jlos_tcp_socket_t* self, uint8_t *data, uint16_t size);
void jlos_tcp_socket_disconnect(jlos_tcp_socket_t* self);
bool jlos_tcp_receive_enqueue(jlos_tcp_socket_t* self, const uint8_t *data, uint32_t size);

uint32_t jlos_tcp_initial_sequence(jlos_tcp_socket_t *socket);

void jlos_tcp_provider_init(jlos_tcp_provider_t* self, jlos_internet_protocol_provider_t *backend);
void jlos_tcp_provider_destroy(jlos_tcp_provider_t* self);
jlos_tcp_socket_t *jlos_tcp_provider_connect(jlos_tcp_provider_t* self, uint32_t ip, uint16_t port);
jlos_tcp_socket_t *jlos_tcp_provider_listen(jlos_tcp_provider_t* self, uint16_t port);
void jlos_tcp_provider_disconnect(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket);
void jlos_tcp_provider_send(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, uint8_t *data, uint16_t size, uint16_t flags);

jlos_tcp_socket_t *jlos_tcp_provider_accept(jlos_tcp_provider_t *self, jlos_tcp_socket_t *listener);

#endif
