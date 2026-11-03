/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <net/tcp.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "tcp"
#include <kernel/printk.h>


static int jlos_tcp_release_timer_expire(jlos_hrtimer_t *timer)
{
    jlos_tcp_socket_t *socket = container_of(timer, jlos_tcp_socket_t, release_timer);
    if (socket->state != JLOS_TCP_TIME_WAIT && socket->state != JLOS_TCP_CLOSED && socket->state != JLOS_TCP_SYN_RECEIVED) {
        return 0;
    }
    jlos_tcp_provider_t *self = socket->backend;
    jlos_hash_chain_remove(&self->sockets, &socket->hash_node);
    if (self->num_sockets > 0) {
        self->num_sockets--;
    }
    jlos_kfree(socket);
    return 0;
}

static void jlos_tcp_schedule_release(jlos_tcp_socket_t *socket, uint64_t delay_ns)
{
    jlos_hrtimer_start(jlos_hrtimer_get_base(), &socket->release_timer, delay_ns, 0, JLOS_HRTIMER_MODE_REL);
}

void jlos_tcp_socket_init(jlos_tcp_socket_t* self, jlos_tcp_provider_t *backend)
{
    self->backend = backend;
    self->state = JLOS_TCP_CLOSED;
    jlos_cond_init(&self->state_cond);
    jlos_mutex_init(&self->state_mutex);
    jlos_semaphore_init(&self->state_event, 0);
    jlos_hrtimer_init(&self->release_timer, jlos_tcp_release_timer_expire, self);
    jlos_list_init(&self->receive_queue);
    jlos_spinlock_init(&self->receive_lock);
    jlos_semaphore_init(&self->receive_wait, 0);
    self->receive_bytes = 0;
    self->send = jlos_tcp_socket_send;
    self->disconnect = jlos_tcp_socket_disconnect;
}

void jlos_tcp_socket_destroy(jlos_tcp_socket_t* self)
{
    while (!jlos_list_empty(&self->receive_queue)) {
        jlos_net_sk_buff_t *skb = container_of(self->receive_queue.next, jlos_net_sk_buff_t, list);
        jlos_list_del_init(&skb->list);
        jlos_net_skb_free(skb);
    }
}

bool jlos_tcp_receive_enqueue(jlos_tcp_socket_t* self, const uint8_t *data, uint32_t size)
{
    jlos_net_sk_buff_t *skb = jlos_net_skb_alloc(size);
    if (!skb) {
        printk_err("tcp rx skb alloc failed\n");
        return false;
    }
    uint8_t *payload = jlos_net_skb_put(skb, size);
    jlos_memcpy(payload, data, size);
    uint32_t fl = jlos_spin_lock_irqsave(&self->receive_lock);
    if (self->receive_bytes + size > JLOS_TCP_RCVBUF_SIZE) {
        jlos_spin_unlock_irqrestore(&self->receive_lock, fl);
        jlos_net_skb_free(skb);
        return false;
    }
    jlos_list_add_tail(&skb->list, &self->receive_queue);
    self->receive_bytes += size;
    jlos_spin_unlock_irqrestore(&self->receive_lock, fl);

    jlos_semaphore_post(&self->receive_wait);
    return true;
}

void jlos_tcp_socket_send(jlos_tcp_socket_t* self, uint8_t *data, uint16_t size)
{
    if (self->state != JLOS_TCP_ESTABLISHED) {
        return;
    }
    jlos_tcp_provider_send(self->backend, self, data, size, (JLOS_TCP_PSH | JLOS_TCP_ACK));
}

void jlos_tcp_socket_disconnect(jlos_tcp_socket_t* self)
{
    jlos_tcp_provider_disconnect(self->backend, self);
}

static uint32_t tcp_hash_port(const void *key)
{
    jlos_tcp_key_t *k = (jlos_tcp_key_t *)key;
    return k->port;
}

static int tcp_cmp_ip_port(const void *key, const void *node)
{
    uint16_t port = ((jlos_tcp_key_t *)key)->port;
    uint32_t ip = ((jlos_tcp_key_t *)key)->ip;
    jlos_tcp_socket_t *socket = container_of(node, jlos_tcp_socket_t, hash_node);
    if (port == socket->local_port && ip == socket->local_ip) {
        return 0;
    }
    return -1;
}

static int tcp_cmp_listener_port(const void *key, const void *node)
{
    jlos_tcp_socket_t *socket = container_of(node, jlos_tcp_socket_t, hash_node);
    if (*(const uint16_t *)key == socket->local_port) {
        return 0;
    }
    return -1;
}

static int tcp_match_listener(jlos_hash_node_t *node, void *args1)
{
    jlos_tcp_socket_t *socket = container_of(node, jlos_tcp_socket_t, hash_node);
    if (socket->state == JLOS_TCP_LISTEN && socket->local_port == (uint16_t)(uintptr_t)args1) {
        return 0;
    }
    return -1;
}

static int tcp_match_socket(jlos_hash_node_t *node, void *args1)
{
    jlos_tcp_socket_t *socket = container_of(node, jlos_tcp_socket_t, hash_node);
    uint32_t *args = args1;
    if (socket->local_ip != args[3] && socket->local_ip != JLOS_IPV4_ANY) {
        return -1;
    }
    if (socket->local_port != (uint16_t)args[4]) {
        return -1;
    }
    if (socket->state == JLOS_TCP_CLOSED) {
        return -1;
    }
    if (socket->remote_ip == args[0] && socket->remote_port == (uint16_t)args[1]) {
        return 0;
    }
    return -1;
}

static bool jlos_tcp_provider_on_internet_protocol_received(jlos_tcp_provider_t* self, jlos_net_sk_buff_t *skb)
{
    uint32_t size = jlos_net_skb_len(skb);
    if (size < 20) {
        return false;
    }
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)skb->data;
    uint16_t flags = JLOS_TCP_GET_FLAGS(msg);
    uint8_t data_offset = JLOS_TCP_GET_DATA_OFFSET(msg);

    jlos_tcp_socket_t *socket = nullptr;
    jlos_tcp_key_t key = {skb->dst_ip, msg->dst_port};
    uint32_t args[] = {skb->src_ip, msg->src_port, flags, skb->dst_ip, msg->dst_port};
    jlos_hash_node_t *node = jlos_hash_chain_find(&self->sockets, &key, tcp_match_socket, args);
    if (node) {
        socket = container_of(node, jlos_tcp_socket_t, hash_node);
    } else {
        node = jlos_hash_chain_find(&self->listeners, &msg->dst_port, tcp_match_listener, (void *)(uintptr_t)msg->dst_port);
        if (node) {
            socket = container_of(node, jlos_tcp_socket_t, hash_node);
        }
    }
    bool reset = false;
    if (socket && (flags & JLOS_TCP_RST)) {
        socket->state = JLOS_TCP_CLOSED;
    }
    if (socket && socket->state == JLOS_TCP_TIME_WAIT) {
        if (flags & JLOS_TCP_FIN) {
            jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
        }
    } else if (socket && socket->state != JLOS_TCP_CLOSED) {
        switch (flags & (JLOS_TCP_SYN | JLOS_TCP_ACK | JLOS_TCP_FIN)) {
            case JLOS_TCP_SYN:
                if (socket->state == JLOS_TCP_LISTEN) {
                    jlos_tcp_socket_t *conn = (jlos_tcp_socket_t *)jlos_kalloc(sizeof(jlos_tcp_socket_t));
                    if (!conn) {
                        break;
                    }
                    jlos_tcp_socket_init(conn, self);
                    conn->state = JLOS_TCP_SYN_RECEIVED;
                    conn->listener = socket;
                    conn->local_port = socket->local_port;
                    conn->local_ip = skb->dst_ip;
                    conn->remote_port = msg->src_port;
                    conn->remote_ip = skb->src_ip;
                    conn->acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
                    conn->sequence_number = JLOS_TCP_INITIAL_SEQUENCE;
                    jlos_tcp_key_t key = {conn->local_ip, conn->local_port};
                    jlos_hash_chain_insert(&self->sockets, &key, &conn->hash_node);
                    self->num_sockets++;
                    jlos_tcp_schedule_release(conn, JLOS_TCP_SYNRECV_RELEASE_NS);

                    jlos_tcp_provider_send(self, conn, 0, 0, (JLOS_TCP_SYN | JLOS_TCP_ACK));
                } else {
                    reset = true;
                }
                break;
            case (JLOS_TCP_SYN | JLOS_TCP_ACK):
                if (socket->state == JLOS_TCP_SYN_SENT) {
                    socket->state = JLOS_TCP_ESTABLISHED;
                    socket->acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
                    socket->sequence_number = JLOS_SWAP_ENDIAN_32(msg->acknowledgement_number);
                    jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                    jlos_semaphore_post(&socket->state_event);
                } else {
                    reset = true;
                }
                break;
            case (JLOS_TCP_SYN | JLOS_TCP_FIN):
            case (JLOS_TCP_SYN | JLOS_TCP_FIN | JLOS_TCP_ACK):
                reset = true;
                break;
            case JLOS_TCP_FIN:
            case (JLOS_TCP_FIN | JLOS_TCP_ACK):
                socket->acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
                switch (socket->state) {
                    case JLOS_TCP_ESTABLISHED:
                        socket->state = JLOS_TCP_CLOSE_WAIT;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        break;
                    case JLOS_TCP_CLOSE_WAIT:
                    case JLOS_TCP_LAST_ACK:
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        break;
                    case JLOS_TCP_FIN_WAIT1:
                        if (flags & JLOS_TCP_ACK) {
                            socket->state = JLOS_TCP_TIME_WAIT;
                            jlos_tcp_schedule_release(socket, JLOS_TCP_TIMEWAIT_TIMEOUT_NS);
                        } else {
                            socket->state = JLOS_TCP_CLOSING;
                        }
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        break;
                    case JLOS_TCP_FIN_WAIT2:
                        socket->state = JLOS_TCP_TIME_WAIT;
                        jlos_tcp_schedule_release(socket, JLOS_TCP_TIMEWAIT_TIMEOUT_NS);
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        break;
                    default:
                        reset = true;
                }
                break;
            case JLOS_TCP_ACK: {
                bool skip_payload = false;
                switch (socket->state) {
                    case JLOS_TCP_CLOSED:
                    case JLOS_TCP_LISTEN:
                    case JLOS_TCP_SYN_SENT:
                    case JLOS_TCP_ESTABLISHED:
                    case JLOS_TCP_FIN_WAIT2:
                    case JLOS_TCP_CLOSE_WAIT:
                        break;
                    case JLOS_TCP_SYN_RECEIVED:
                        socket->state = JLOS_TCP_ESTABLISHED;
                        socket->sequence_number = JLOS_SWAP_ENDIAN_32(msg->acknowledgement_number);
                        uint32_t queued = jlos_list_count(&self->accept_queue);
                        if (queued < JLOS_TCP_ACCEPT_BACKLOG_MAX) {
                            uint32_t fl = jlos_spin_lock_irqsave(&self->accept_lock);
                            jlos_list_add_tail(&socket->accept_node, &self->accept_queue);
                            jlos_spin_unlock_irqrestore(&self->accept_lock, fl);

                            jlos_semaphore_post(&socket->listener->state_event);
                        }
                        return false;
                    case JLOS_TCP_FIN_WAIT1:
                        socket->state = JLOS_TCP_FIN_WAIT2;
                        jlos_semaphore_post(&socket->state_event);
                        return false;
                    case JLOS_TCP_CLOSING:
                        socket->state = JLOS_TCP_TIME_WAIT;
                        jlos_tcp_schedule_release(socket, JLOS_TCP_TIMEWAIT_TIMEOUT_NS);
                        skip_payload = true;
                        break;
                    case JLOS_TCP_LAST_ACK:
                        socket->state = JLOS_TCP_CLOSED;
                        jlos_tcp_schedule_release(socket, JLOS_TCP_CLOSED_RELEASE_NS);
                        skip_payload = true;
                        break;
                    default:
                        break;
                }
                if (skip_payload) {
                    break;
                }
            }
            __attribute__((fallthrough));
            default:
                if (JLOS_SWAP_ENDIAN_32(msg->sequence_number) == socket->acknowledgement_number) {
                    uint32_t header_bytes = data_offset * 4;
                    if (data_offset < JLOS_TCP_MIN_DATA_OFFSET || header_bytes > size) {
                        return false;
                    }
                    uint32_t payload_len = size - header_bytes;
                    printk_debug("payload len=%x%x bytes, calling handler\n",
                        (payload_len >> 8) & 0xFF, payload_len & 0xFF);
                    if (!jlos_tcp_receive_enqueue(socket, (skb->data + header_bytes), payload_len)) {
                        return false;
                    }
                    socket->acknowledgement_number += payload_len;
                    jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                } else {
                    jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                }
        }
    }
    if (reset && (skb->dst_ip & JLOS_IPV4_LOOPBACK_MASK) != JLOS_IPV4_LOOPBACK_NET) {
        printk_debug("sending rst (reset)\n");
        if (socket) {
            jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_RST);
        } else {
            jlos_tcp_socket_t socket1;
            jlos_tcp_socket_init(&socket1, self);
            socket1.remote_port = msg->src_port;
            socket1.remote_ip = skb->src_ip;
            socket1.local_port = msg->dst_port;
            socket1.local_ip = skb->dst_ip;
            socket1.sequence_number = JLOS_SWAP_ENDIAN_32(msg->acknowledgement_number);
            socket1.acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
            jlos_tcp_provider_send(self, &socket1, 0, 0, JLOS_TCP_RST);
            return true;
        }
    }
    if (socket) {
        jlos_cond_broadcast(&socket->state_cond);
        jlos_semaphore_post(&socket->state_event);
    }
    return false;
}

void jlos_tcp_provider_init(jlos_tcp_provider_t* self, jlos_internet_protocol_provider_t *backend)
{
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x06);
    self->base_handler.on_internet_protocol_received =
        (bool (*)(jlos_internet_protocol_handler_t *, jlos_net_sk_buff_t *))jlos_tcp_provider_on_internet_protocol_received;
    self->num_sockets = 0;
    self->free_port = JLOS_EPHEMERAL_PORT_START;
    jlos_hash_chain_init(&self->sockets, JLOS_NET_HASH_CHAIN_NUM, tcp_hash_port, tcp_cmp_ip_port);
    jlos_hash_chain_init(&self->listeners, JLOS_NET_HASH_CHAIN_NUM, jlos_hash_uint16, tcp_cmp_listener_port);
    jlos_list_init(&self->accept_queue);
    jlos_spinlock_init(&self->accept_lock);
}

void jlos_tcp_provider_destroy(jlos_tcp_provider_t* self)
{
    jlos_hash_chain_destroy(&self->sockets);
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

void jlos_tcp_provider_send(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, uint8_t *data, uint16_t size, uint16_t flags)
{
    (void)self;
    uint8_t doff;
    uint16_t tcp_hdr_len;
    if ((flags & JLOS_TCP_SYN) != 0) {
        doff = 6;
        tcp_hdr_len = 24;
    } else {
        doff = 5;
        tcp_hdr_len = 20;
    }
    uint16_t total_length = size + tcp_hdr_len;
    uint32_t max_hdr = sizeof(jlos_ether_frame_header_t) + sizeof(jlos_ipv4_message_t) + tcp_hdr_len;
    jlos_net_sk_buff_t *skb = jlos_net_skb_alloc(size + max_hdr);
    if (!skb) {
        return;
    }
    jlos_net_skb_reserve(skb, max_hdr);
    uint8_t *payload = jlos_net_skb_put(skb, size);
    jlos_memcpy(payload, data, size);
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)jlos_net_skb_push(skb, tcp_hdr_len);
    JLOS_TCP_SET_DATA_OFFSET_FLAGS(msg, doff, flags);
    msg->src_port = socket->local_port;
    msg->dst_port = socket->remote_port;
    msg->acknowledgement_number = JLOS_SWAP_ENDIAN_32(socket->acknowledgement_number);
    msg->sequence_number = JLOS_SWAP_ENDIAN_32(socket->sequence_number);
    msg->window_size = JLOS_SWAP_ENDIAN_16(0xFFFF);
    msg->urgent_ptr = 0;
    if (flags & JLOS_TCP_SYN) {
        uint8_t *opt = (uint8_t *)msg + sizeof(jlos_tcp_header_t);
        opt[0] = JLOS_TCP_MSS_OPTIONS_KIND;
        opt[1] = JLOS_TCP_MSS_OPTION_LEN;
        opt[2] = (uint8_t)(JLOS_TCP_MSS >> 8);
        opt[3] = (uint8_t)(JLOS_TCP_MSS & 0xFF);
    }
    socket->sequence_number += size;
    msg->checksum = 0;
    jlos_tcp_pseudo_header_t phdr;
    phdr.src_ip = socket->local_ip;
    phdr.dst_ip = socket->remote_ip;
    phdr.protocol = 0x0600;
    phdr.total_length = JLOS_SWAP_ENDIAN_16(total_length);
    uint32_t csum = jlos_internet_protocol_provider_csum_partial(&phdr, sizeof(jlos_tcp_pseudo_header_t), 0);
    csum = jlos_internet_protocol_provider_csum_partial(skb->data, jlos_net_skb_len(skb), csum);
    msg->checksum = jlos_internet_protocol_provider_csum_fold(csum);
    jlos_internet_protocol_handler_send(&self->base_handler, socket->remote_ip, skb);
}

jlos_tcp_socket_t *jlos_tcp_provider_connect(jlos_tcp_provider_t* self, uint32_t ip, uint16_t port)
{
    jlos_tcp_socket_t *socket = (jlos_tcp_socket_t *)jlos_kalloc(sizeof(jlos_tcp_socket_t));
    if (socket) {
        jlos_tcp_socket_init(socket, self);
        socket->remote_port = JLOS_SWAP_ENDIAN_16(port);
        socket->remote_ip = ip;
        uint16_t free_port = self->free_port++;
        socket->local_port = JLOS_SWAP_ENDIAN_16(free_port);
        socket->local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        socket->state = JLOS_TCP_SYN_SENT;
        socket->sequence_number = JLOS_TCP_INITIAL_SEQUENCE;
        jlos_tcp_key_t key = {socket->local_ip, socket->local_port};
        jlos_hash_chain_insert(&self->sockets, &key, &socket->hash_node);
        self->num_sockets++;
        socket->state = JLOS_TCP_SYN_SENT;

        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_SYN);
    }
    return socket;
}

void jlos_tcp_provider_disconnect(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket)
{
    if (socket->state == JLOS_TCP_LISTEN) {
        jlos_hash_chain_remove(&self->listeners, &socket->hash_node);
        jlos_kfree(socket);
        return;
    }
    if (socket->state == JLOS_TCP_ESTABLISHED) {
        socket->state = JLOS_TCP_FIN_WAIT1;
        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_FIN | JLOS_TCP_ACK);
        socket->sequence_number++;
    } else if (socket->state == JLOS_TCP_CLOSE_WAIT) {
        socket->state = JLOS_TCP_LAST_ACK;
        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_FIN | JLOS_TCP_ACK);
        socket->sequence_number++;
    }
}

jlos_tcp_socket_t *jlos_tcp_provider_listen(jlos_tcp_provider_t* self, uint16_t port)
{
    jlos_tcp_socket_t *socket = (jlos_tcp_socket_t *)jlos_kalloc(sizeof(jlos_tcp_socket_t));
    if (socket) {
        jlos_tcp_socket_init(socket, self);
        socket->state = JLOS_TCP_LISTEN;
        socket->local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        socket->local_port = JLOS_SWAP_ENDIAN_16(port);
        socket->listener = nullptr;
        jlos_hash_chain_insert(&self->listeners, &socket->local_port, &socket->hash_node);
        self->num_sockets++;
    }
    return socket;
}

jlos_tcp_socket_t *jlos_tcp_provider_accept(jlos_tcp_provider_t *self, jlos_tcp_socket_t *listener)
{
    for (;;) {
        uint32_t fl = jlos_spin_lock_irqsave(&self->accept_lock);
        if (!jlos_list_empty(&self->accept_queue)) {
            jlos_list_head_t *first = self->accept_queue.next;
            jlos_list_del_init(first);
            jlos_spin_unlock_irqrestore(&self->accept_lock, fl);

            return container_of(first, jlos_tcp_socket_t, accept_node);
        }
        jlos_spin_unlock_irqrestore(&self->accept_lock, fl);
        if (listener->state != JLOS_TCP_LISTEN) {
            return nullptr;
        }
        jlos_semaphore_wait(&listener->state_event);
    }
}


