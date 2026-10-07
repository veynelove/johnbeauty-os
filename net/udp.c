/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <net/udp.h>
#include <kernel/memory_manager.h>
#include <include/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "udp"
#include <kernel/printk.h>

void jlos_udp_handler_init(jlos_udp_handler_t* self)
{
    printk_debug("handler initialized\n");
    self->handle_udp_message = jlos_udp_handler_handle_udp_message;
}

void jlos_udp_handler_destroy(jlos_udp_handler_t* self)
{
    (void)self;
}

void jlos_udp_handler_handle_udp_message(jlos_udp_handler_t* self, jlos_udp_socket_t* socket, uint8_t *data, uint16_t size)
{
    (void)self;
    (void)socket;
    (void)data;
    printk_debug("handler received data, size=%x%x\n", size & 0xFF, (size >> 8) & 0xFF);
}

void jlos_udp_socket_init(jlos_udp_socket_t* self, jlos_udp_provider_t *backend)
{
    printk_debug("socket initialized\n");
    self->backend = backend;
    self->handler = NULL;
    self->listening = false;
    self->handle_udp_message = jlos_udp_socket_handle_udp_message;
    self->send = jlos_udp_socket_send;
    self->disconnect = jlos_udp_socket_disconnect;
}

void jlos_udp_socket_destroy(jlos_udp_socket_t* self)
{
    (void)self;
}

void jlos_udp_socket_handle_udp_message(jlos_udp_socket_t* self, uint8_t *data, uint16_t size)
{
    printk_debug("socket received data\n");
    if (self->handler) {
        self->handler->handle_udp_message(self->handler, self, data, size);
    }
}

void jlos_udp_socket_send(jlos_udp_socket_t* self, uint8_t *data, uint16_t size)
{
    printk_debug("socket sending data\n");
    jlos_udp_provider_send(self->backend, self, data, size);
}

void jlos_udp_socket_disconnect(jlos_udp_socket_t* self)
{
    jlos_udp_provider_disconnect(self->backend, self);
}

static uint32_t udp_hash_ip_port(const void *key)
{
    jlos_udp_key_t *k = (jlos_udp_key_t *)key;
    uint32_t port = k->port;
    return k->ip ^ (port << 16) ^ (port >> 16);
}

static int udp_cmp_ip_port(const void *key, const void *node)
{
    uint16_t port = ((jlos_udp_key_t *)key)->port;
    uint32_t ip = ((jlos_udp_key_t *)key)->ip;
    jlos_udp_socket_t *socket = container_of(node, jlos_udp_socket_t, hash_node);
    if (port == socket->local_port && ip == socket->local_ip) {
        return 0;
    }
    return -1;
}

static int udp_match_socket(jlos_hash_node_t *node, void *args1)
{
    jlos_udp_socket_t *socket = container_of(node, jlos_udp_socket_t, hash_node);
    uint32_t *args = args1;
    if (socket->local_port != (uint16_t)args[3]) {
        return -1;
    }
    if (socket->local_ip != args[2] && args[2] != JLOS_IPV4_BROADCAST) {
        return -1;
    }
    if (socket->remote_ip == args[0] && socket->remote_port == (uint16_t)args[1]) {
        return 0;
    }
    if (socket->listening) {
        socket->remote_ip = args[0];
        socket->remote_port = (uint16_t)args[1];
        return 0;
    }
    return -1;
}

static bool jlos_udp_provider_on_internet_protocol_received(jlos_udp_provider_t* self, jlos_net_sk_buff_t *skb)
{
    printk_debug("received packet\n");
    uint32_t size = jlos_net_skb_len(skb);
    if (size < sizeof(jlos_udp_header_t)) {
        printk_debug("packet too small\n");
        return false;
    }
    jlos_udp_header_t *msg = (jlos_udp_header_t *)skb->data;
    printk_debug("destination port=%x%x\n", msg->dst_port & 0xFF, (msg->dst_port >> 8) & 0xFF);

    jlos_udp_socket_t *socket = NULL;
    jlos_udp_key_t key = {skb->dst_ip, msg->dst_port};
    uint32_t args[] = {skb->src_ip, msg->src_port, skb->dst_ip, msg->dst_port};
    jlos_hash_node_t *node = jlos_hash_chain_find(&self->sockets, &key, udp_match_socket, args);
    if (!node && skb->dst_ip == JLOS_IPV4_BROADCAST) {
        jlos_udp_key_t bkey = {0, msg->dst_port};
        node = jlos_hash_chain_find(&self->sockets, &bkey, udp_match_socket, args);
    }
    if (node) {
        socket = container_of(node, jlos_udp_socket_t, hash_node);
        printk_debug("socket matched\n");
    }
    if (socket) {
        socket->handle_udp_message(socket, skb->data + sizeof(jlos_udp_header_t), size - sizeof(jlos_udp_header_t));
        return true;
    }
    printk_debug("socket not matched\n");
    return false;
}

void jlos_udp_provider_init(jlos_udp_provider_t* self, jlos_internet_protocol_provider_t *backend)
{
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x11);
    self->base_handler.on_internet_protocol_received =
        (bool (*)(jlos_internet_protocol_handler_t *, jlos_net_sk_buff_t *))jlos_udp_provider_on_internet_protocol_received;
    self->num_sockets = 0;
    self->free_port = JLOS_EPHEMERAL_PORT_START;
    jlos_hash_chain_init(&self->sockets, JLOS_NET_HASH_CHAIN_NUM, udp_hash_ip_port, udp_cmp_ip_port);
    printk_info("initialized\n");
}

void jlos_udp_provider_destroy(jlos_udp_provider_t* self)
{
    jlos_hash_chain_destroy(&self->sockets);
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

jlos_udp_socket_t *jlos_udp_provider_connect(jlos_udp_provider_t* self, uint32_t ip, uint16_t port)
{
    jlos_udp_socket_t *socket = (jlos_udp_socket_t *)jlos_kalloc(sizeof(jlos_udp_socket_t));
    if (socket) {
        jlos_udp_socket_init(socket, self);
        socket->remote_port = JLOS_SWAP_ENDIAN_16(port);
        socket->remote_ip = ip;
        uint16_t free_port = self->free_port++;
        socket->local_port = JLOS_SWAP_ENDIAN_16(free_port);
        socket->local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        jlos_udp_key_t key = {socket->local_ip, socket->local_port};
        jlos_hash_chain_insert(&self->sockets, &key, &socket->hash_node);
        self->num_sockets++;
    }
    return socket;
}

jlos_udp_socket_t *jlos_udp_provider_listen(jlos_udp_provider_t* self, uint16_t port)
{
    jlos_udp_socket_t *socket = (jlos_udp_socket_t *)jlos_kalloc(sizeof(jlos_udp_socket_t));
    if (socket) {
        jlos_udp_socket_init(socket, self);
        socket->listening = true;
        socket->local_port = JLOS_SWAP_ENDIAN_16(port);
        socket->local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        jlos_udp_key_t key = {socket->local_ip, socket->local_port};
        jlos_hash_chain_insert(&self->sockets, &key, &socket->hash_node);
        self->num_sockets++;
    }
    return socket;
}

void jlos_udp_provider_disconnect(jlos_udp_provider_t* self, jlos_udp_socket_t *socket)
{
    jlos_hash_chain_remove(&self->sockets, &socket->hash_node);
    self->num_sockets--;
    jlos_kfree(socket);
}

void jlos_udp_provider_send(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, uint8_t *data, uint16_t size)
{
    uint32_t max_hdr = sizeof(jlos_ether_frame_header_t) + sizeof(jlos_ipv4_message_t) + sizeof(jlos_udp_header_t);
    jlos_net_sk_buff_t *skb = jlos_net_skb_alloc(size + max_hdr);
    if (!skb) {
        return;
    }
    jlos_net_skb_reserve(skb, max_hdr);
    uint8_t *payload = jlos_net_skb_put(skb, size);
    jlos_memcpy(payload, data, size);
    jlos_udp_header_t *msg = (jlos_udp_header_t *)jlos_net_skb_push(skb, sizeof(jlos_udp_header_t));
    msg->src_port = socket->local_port;
    msg->dst_port = socket->remote_port;
    msg->length = JLOS_SWAP_ENDIAN_16(size + sizeof(jlos_udp_header_t));
    msg->checksum = 0;
    jlos_internet_protocol_handler_send(&self->base_handler, socket->remote_ip, skb);
}

void jlos_udp_provider_bind(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, jlos_udp_handler_t *handler)
{
    (void)self;
    socket->handler = handler;
}
