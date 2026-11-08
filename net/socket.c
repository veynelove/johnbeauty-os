/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <net/socket.h>
#include <net/network.h>
#include <net/skbuff.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "socket"
#include <kernel/printk.h>

static bool jlos_socket_receive_enqueue(jlos_socket_t *self, const uint8_t *data, uint32_t size)
{
    jlos_net_sk_buff_t *skb = jlos_net_skb_alloc(size);
    if (!skb) {
        printk_err("rx skb alloc failed\n");
        return false;
    }
    uint8_t *payload =  jlos_net_skb_put(skb, size);
    jlos_memcpy(payload, data, size);
    uint32_t fl = jlos_spin_lock_irqsave(&self->recv_lock);
    if (self->closed || self->recv_queue_bytes + size > JLOS_SOCKET_RECVBUF_SIZE) {
        jlos_spin_unlock_irqrestore(&self->recv_lock, fl);
        jlos_net_skb_free(skb);
        return false;
    }
    jlos_list_add_tail(&skb->list, &self->recv_queue);
    self->recv_queue_bytes += size;
    jlos_spin_unlock_irqrestore(&self->recv_lock, fl);
    jlos_semaphore_post(&self->recv_wait);
    return true;
}


static void jlos_socket_udp_receive(jlos_udp_handler_t *handler, jlos_udp_socket_t *udp_socket, uint8_t *data, uint16_t size)
{
    jlos_socket_t *self = container_of(handler, jlos_socket_t, udp_handler);
    (void)udp_socket;
    jlos_socket_receive_enqueue(self, data, size);
}

jlos_socket_t *jlos_socket_create(uint32_t domain, uint32_t type, uint32_t protocol)
{
    if (domain != JLOS_SOCKET_DOMAIN_INET) {
        printk_err("unsupported domain\n", domain);
        return nullptr;
    }
    if (type != JLOS_SOCKET_STREAM && type != JLOS_SOCKET_DGRAM) {
        printk_err("unsupported type = %u\n", type);
        return nullptr;
    }
    jlos_socket_t *self = (jlos_socket_t *)jlos_kalloc(sizeof(jlos_socket_t));
    if (!self) {
        return nullptr;
    }
    jlos_memset(self, 0, sizeof(jlos_socket_t));
    self->domain = domain;
    self->type = type;
    self->protocol = protocol;

    self->udp_handler.handle_udp_message = jlos_socket_udp_receive;
    jlos_list_init(&self->recv_queue);
    jlos_spinlock_init(&self->recv_lock);
    jlos_semaphore_init(&self->recv_wait, 0);
    return self;
}

int32_t jlos_socket_bind(jlos_socket_t *self, uint32_t ip, uint16_t port)
{
    if (!self || self->closed || self->bound) {
        return -1;
    }
    self->local_ip = ip;
    self->local_port = port;
    self->bound = true;
    return 0;
}

int32_t jlos_socket_listen(jlos_socket_t *self, uint32_t backlog)
{
    (void)backlog;
    if (!self || self->closed || self->listening || !self->bound || !g_network_stack) {
        return -1;
    }
    if (self->type == JLOS_SOCKET_STREAM) {
        self->proto.tcp = jlos_tcp_provider_listen(&g_network_stack->tcp, self->local_port);
        if (!self->proto.tcp) {
            printk_err("tcp listen failed port = %u\n", self->local_port);
            return -1;
        }

    } else {
        self->proto.udp = jlos_udp_provider_listen(&g_network_stack->udp, self->local_port);
        if (!self->proto.udp) {
            printk_err("udp listen failed port = %u\n", self->local_port);
            return -1;
        }
        jlos_udp_provider_bind(&g_network_stack->udp, self->proto.udp, &self->udp_handler);
    }
    self->listening = true;
    return 0;
}

int32_t jlos_socket_connect(jlos_socket_t *self, uint32_t ip, uint16_t port)
{
    if (!self || self->closed || !g_network_stack) {
        return -1;
    }
    if (self->type == JLOS_SOCKET_STREAM) {
        self->proto.tcp = jlos_tcp_provider_connect(&g_network_stack->tcp, ip, port);
        if (!self->proto.tcp) {
            printk_err("tcp connet failed\n");
            return -1;
        }

        while (self->proto.tcp->state == JLOS_TCP_SYN_SENT) {
            jlos_semaphore_wait(&self->proto.tcp->state_event);
        }
        if (self->proto.tcp->state != JLOS_TCP_ESTABLISHED) {
            return -1;
        }
    } else {
        self->proto.udp = jlos_udp_provider_connect(&g_network_stack->udp, ip, port);
        if (!self->proto.udp) {
            printk_err("udp connet failed\n");
            return -1;
        }
        jlos_udp_provider_bind(&g_network_stack->udp, self->proto.udp, &self->udp_handler);
    }
    return 0;
}

jlos_socket_t *jlos_socket_accept(jlos_socket_t *self)
{
    if (!self || self->closed || !self->listening || self->type != JLOS_SOCKET_STREAM || !self->proto.tcp) {
        return nullptr;
    }
    jlos_tcp_socket_t *conn = jlos_tcp_provider_accept(&g_network_stack->tcp, self->proto.tcp);
    if (!conn) {
        return nullptr;
    }
    jlos_socket_t *child = jlos_socket_create(self->domain, self->type, self->protocol);
    if (!child) {
        jlos_kfree(conn);
        return nullptr;
    }
    child->bound = true;
    child->listening = false;
    child->local_ip = conn->local_ip;
    child->local_port = conn->local_port;
    child->proto.tcp = conn;

    return child;
}

int32_t jlos_socket_send(jlos_socket_t *self, const void *buf, uint32_t len)
{
    if (!self || self->closed) {
        return -1;
    }
    if (self->type == JLOS_SOCKET_STREAM) {
        if (!self->proto.tcp || self->proto.tcp->state != JLOS_TCP_ESTABLISHED) {
            return -1;
        }
        uint32_t sent = 0;
        while (sent < len) {
            uint32_t chunk = len - sent;
            if (chunk > JLOS_TCP_MSS) {
                chunk = JLOS_TCP_MSS;
            }

            jlos_tcp_provider_send(&g_network_stack->tcp, self->proto.tcp, (uint8_t *)buf + sent, (uint16_t)chunk, (JLOS_TCP_PSH | JLOS_TCP_ACK));
            sent += chunk;
        }
        return (int32_t)len;
    }
    if (!self->proto.udp || len > 0xFFFF) {
        return -1;
    }
    jlos_udp_provider_send(&g_network_stack->udp, self->proto.udp, (uint8_t *)buf, (uint16_t)len);
    return (int32_t)len;
}

int32_t jlos_socket_recv(jlos_socket_t *self, void *buf, uint32_t len)
{
    if (!self || self->closed || len == 0) {
        return -1;
    }
    if (self->type == JLOS_SOCKET_STREAM) {
        jlos_tcp_socket_t *tcp = self->proto.tcp;
        if (!tcp) {
            return -1;
        }
        for (;;) {
            uint32_t fl = jlos_spin_lock_irqsave(&tcp->receive_lock);
            bool empty = jlos_list_empty(&tcp->receive_queue);
            jlos_spin_unlock_irqrestore(&tcp->receive_lock, fl);
            if (!empty) {
                break;
            }
            if (self->closed) {
                return -1;
            }

            jlos_semaphore_wait(&tcp->receive_wait);
        }
        uint32_t fl = jlos_spin_lock_irqsave(&tcp->receive_lock);
        jlos_net_sk_buff_t *skb = container_of(tcp->receive_queue.next, jlos_net_sk_buff_t, list);
        uint32_t avail = jlos_net_skb_len(skb);
        uint32_t n = avail < len ? avail : len;
        jlos_memcpy(buf, skb->data, n);
        tcp->receive_bytes -= n;
        if (n < avail) {
            jlos_net_skb_pull(skb, n);
        } else {
            jlos_list_del_init(&skb->list);
        }
        jlos_spin_unlock_irqrestore(&tcp->receive_lock, fl);
        if (n == avail) {
            jlos_net_skb_free(skb);
        }
        return (int32_t)n;
    }
    for (;;) {
        uint32_t fl = jlos_spin_lock_irqsave(&self->recv_lock);
        bool empty = jlos_list_empty(&self->recv_queue);
        jlos_spin_unlock_irqrestore(&self->recv_lock, fl);
        if (!empty) {
            break;
        }
        if (self->closed) {
            return -1;
        }
        jlos_semaphore_wait(&self->recv_wait);
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->recv_lock);
    jlos_net_sk_buff_t *skb = container_of(self->recv_queue.next, jlos_net_sk_buff_t, list);
    uint32_t avail = jlos_net_skb_len(skb);
    uint32_t n = avail < len ? avail : len;
    jlos_memcpy(buf, skb->data, n);
    self->recv_queue_bytes -= n;
    if (n < avail) {
        jlos_net_skb_pull(skb, n);
    } else {
        jlos_list_del_init(&skb->list);
    }
    jlos_spin_unlock_irqrestore(&self->recv_lock, fl);
    if (n == avail) {
        jlos_net_skb_free(skb);
    }
    return (int32_t)n;
}

void jlos_socket_close(jlos_socket_t *self)
{
    if (!self || self->closed) {
        return;
    }
    self->closed = true;
    if (self->proto.tcp) {
        jlos_semaphore_post(&self->proto.tcp->receive_wait);
        jlos_tcp_provider_disconnect(&g_network_stack->tcp, self->proto.tcp);
        self->proto.tcp = nullptr;
    }
    if (self->proto.udp) {
        jlos_udp_provider_disconnect(&g_network_stack->udp, self->proto.udp);
        self->proto.udp = nullptr;
    }
    jlos_semaphore_post(&self->recv_wait);
}

void jlos_socket_destroy(jlos_socket_t *self)
{
    if (!self) {
        return;
    }
    jlos_socket_close(self);
    while (!jlos_list_empty(&self->recv_queue)) {
        jlos_net_sk_buff_t *skb = container_of(self->recv_queue.next, jlos_net_sk_buff_t, list);
        jlos_list_del_init(&skb->list);
        jlos_net_skb_free(skb);
    }
    jlos_kfree(self);
}

void jlos_socket_getpeer_name(jlos_socket_t *self, uint32_t *ip, uint16_t *port)
{
    if (!self || !ip || !port) {
        return;
    }
    if (self->type == JLOS_SOCKET_STREAM && self->proto.tcp) {
        *ip = self->proto.tcp->remote_ip;
        *port = self->proto.tcp->remote_port;
    } else if (self->type == JLOS_SOCKET_DGRAM && self->proto.udp) {
        *ip = self->proto.udp->remote_ip;
        *port = self->proto.udp->remote_port;
    }
}
