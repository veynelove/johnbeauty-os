#include <net/tcp.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "tcp"

void jlos_tcp_handler_init(jlos_tcp_handler_t* self)
{
    self->handle_tcp_message = jlos_tcp_handler_handle_tcp_message;
}

void jlos_tcp_handler_destroy(jlos_tcp_handler_t* self)
{
    (void)self;
}

bool jlos_tcp_handler_handle_tcp_message(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *data, uint16_t size)
{
    (void)self;
    (void)socket;
    (void)data;
    (void)size;
    return true;
}

void jlos_tcp_socket_init(jlos_tcp_socket_t* self, jlos_tcp_provider_t *backend)
{
    self->backend = backend;
    self->handler = NULL;
    self->state = JLOS_TCP_CLOSED;
    self->handle_tcp_message = jlos_tcp_socket_handle_tcp_message;
    self->send = jlos_tcp_socket_send;
    self->disconnect = jlos_tcp_socket_disconnect;
}

void jlos_tcp_socket_destroy(jlos_tcp_socket_t* self)
{
    (void)self;
}

bool jlos_tcp_socket_handle_tcp_message(jlos_tcp_socket_t* self, uint8_t *data, uint16_t size)
{
    if (self->handler) {
        return self->handler->handle_tcp_message(self->handler, self, data, size);
    }
    return false;
}

void jlos_tcp_socket_send(jlos_tcp_socket_t* self, uint8_t *data, uint16_t size)
{
    while (self->state != JLOS_TCP_ESTABLISHED) {}
    jlos_tcp_provider_send(self->backend, self, data, size, (JLOS_TCP_PSH | JLOS_TCP_ACK));
}

void jlos_tcp_socket_disconnect(jlos_tcp_socket_t* self)
{
    jlos_tcp_provider_disconnect(self->backend, self);
}

static uint32_t tcp_hash_ip_port(const void *key)
{
    jlos_tcp_key_t *k = (jlos_tcp_key_t *)key;
    uint32_t port = k->port;
    return k->ip ^ (port << 16) ^ (port >> 16);
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

void jlos_tcp_provider_init(jlos_tcp_provider_t* self, jlos_internet_protocol_provider_t *backend)
{
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x06);
    self->base_handler.on_internet_protocol_received =
        (bool (*)(jlos_internet_protocol_handler_t*, uint32_t, uint32_t, uint8_t*, uint32_t))jlos_tcp_provider_on_internet_protocol_received;
    self->num_sockets = 0;
    self->free_port = 1024;
    jlos_hash_chain_init(&self->sockets, JLOS_NET_HASH_CHAIN_NUM, tcp_hash_ip_port, tcp_cmp_ip_port);
}

void jlos_tcp_provider_destroy(jlos_tcp_provider_t* self)
{
    jlos_hash_chain_destroy(&self->sockets);
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

static int tcp_match_socket(jlos_hash_node_t *node, void *args1)
{
    jlos_tcp_socket_t *socket = container_of(node, jlos_tcp_socket_t, hash_node);
    uint32_t *args = args1;
    if (socket->local_ip != args[3] || socket->local_port != (uint16_t)args[4]) {
        return -1;
    }
    if (socket->state == JLOS_TCP_LISTEN && ((uint16_t)args[2] & (JLOS_TCP_SYN | JLOS_TCP_ACK)) == JLOS_TCP_SYN) {
        return 0;
    }
    if (socket->remote_ip == args[0] && socket->remote_port == (uint16_t)args[1]) {
        return 0;
    }
    return -1;
}

static int tcp_match_closed_socket(jlos_hash_node_t *node, void *arg)
{
    jlos_tcp_socket_t *socket = container_of(node, jlos_tcp_socket_t, hash_node);
    return (socket == arg) ? 0 : -1;
}

bool jlos_tcp_provider_on_internet_protocol_received(jlos_tcp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE,
    uint8_t *internet_protocol_payload, uint32_t size)
{
    if (size < 20) {
        return false;
    }
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)internet_protocol_payload;
    uint16_t flags = JLOS_TCP_GET_FLAGS(msg);
    uint8_t data_offset = JLOS_TCP_GET_DATA_OFFSET(msg);
    printk_debug("recv %x%x->%x%x flags=%x seq=%x ack=%x\n",
        (JLOS_SWAP_ENDIAN_16(msg->src_port) >> 8) & 0xFF,
        JLOS_SWAP_ENDIAN_16(msg->src_port) & 0xFF,
        (JLOS_SWAP_ENDIAN_16(msg->dst_port) >> 8) & 0xFF,
        JLOS_SWAP_ENDIAN_16(msg->dst_port) & 0xFF,
        flags,
        JLOS_SWAP_ENDIAN_32(msg->sequence_number),
        JLOS_SWAP_ENDIAN_32(msg->acknowledgement_number));
    jlos_tcp_socket_t *socket = NULL;
    jlos_tcp_key_t key = {dstIP_BE, msg->dst_port};
    uint32_t args[] = {srcIP_BE, msg->src_port, flags, dstIP_BE, msg->dst_port};
    jlos_hash_node_t *node = jlos_hash_chain_find(&self->sockets, &key, tcp_match_socket, args);
    if (node) {
        socket = container_of(node, jlos_tcp_socket_t, hash_node);
    }
    bool reset = false;
    if (socket && (flags & JLOS_TCP_RST)) {
        socket->state = JLOS_TCP_CLOSED;
    }
    if (socket && socket->state != JLOS_TCP_CLOSED) {
        printk_debug("socket state=%x remote=%x.%x.%x.%x:%x%x\n",
            socket->state,
            socket->remote_ip & 0xFF,
            (socket->remote_ip >> 8) & 0xFF,
            (socket->remote_ip >> 16) & 0xFF,
            (socket->remote_ip >> 24) & 0xFF,
            (JLOS_SWAP_ENDIAN_16(socket->remote_port) >> 8) & 0xFF,
            JLOS_SWAP_ENDIAN_16(socket->remote_port) & 0xFF);
        switch (flags & (JLOS_TCP_SYN | JLOS_TCP_ACK | JLOS_TCP_FIN)) {
            case JLOS_TCP_SYN:
                if (socket->state == JLOS_TCP_LISTEN) {
                    socket->state = JLOS_TCP_SYN_RECEIVED;
                    socket->remote_port = msg->src_port;
                    socket->remote_ip = srcIP_BE;
                    socket->acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
                    socket->sequence_number = 0xbeefcafe;
                    printk_debug("listen -> syn_rcvd, sending syn-ack\n");
                    jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_SYN | JLOS_TCP_ACK));
                } else if (socket->state == JLOS_TCP_SYN_RECEIVED) {
                    printk_debug("syn_rcvd retransmitting syn-ack (lost?)\n");
                    jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_SYN | JLOS_TCP_ACK));
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
                switch (socket->state) {
                    case JLOS_TCP_ESTABLISHED:
                        socket->state = JLOS_TCP_CLOSE_WAIT;
                        socket->acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_FIN | JLOS_TCP_ACK));
                        break;
                    case JLOS_TCP_CLOSE_WAIT:
                        socket->state = JLOS_TCP_CLOSED;
                        break;
                    case JLOS_TCP_FIN_WAIT1:
                    case JLOS_TCP_FIN_WAIT2:
                        socket->state = JLOS_TCP_CLOSED;
                        socket->acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        break;
                    default:
                        reset = true;
                }
                break;
            case JLOS_TCP_ACK:
                switch (socket->state) {
                    case JLOS_TCP_CLOSED:
                    case JLOS_TCP_LISTEN:
                    case JLOS_TCP_SYN_SENT:
                    case JLOS_TCP_ESTABLISHED:
                    case JLOS_TCP_FIN_WAIT2:
                        break;
                    case JLOS_TCP_SYN_RECEIVED:
                        socket->state = JLOS_TCP_ESTABLISHED;
                        socket->sequence_number = JLOS_SWAP_ENDIAN_32(msg->acknowledgement_number);
                        printk_debug("3-way handshake complete, state=established\n");
                        return false;
                    case JLOS_TCP_FIN_WAIT1:
                        socket->state = JLOS_TCP_FIN_WAIT2;
                        return false;
                    case JLOS_TCP_CLOSE_WAIT:
                        socket->state = JLOS_TCP_CLOSED;
                        break;
                    default:
                        break;
                }
                __attribute__((fallthrough));
            default:
                if (JLOS_SWAP_ENDIAN_32(msg->sequence_number) == socket->acknowledgement_number) {
                    uint32_t header_bytes = data_offset * 4;
                    uint32_t payload_len = size - header_bytes;
                    printk_debug("payload len=%x%x bytes, calling handler\n",
                        (payload_len >> 8) & 0xFF, payload_len & 0xFF);
                    reset = !socket->handle_tcp_message(socket, (internet_protocol_payload + header_bytes), payload_len);
                    if (!reset) {
                        socket->acknowledgement_number += payload_len;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                    }
                } else {
                    reset = true;
                }
        }
    }
    if (reset) {
        printk_debug("sending rst (reset)\n");
        if (socket) {
            jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_RST);
        } else {
            jlos_tcp_socket_t socket1;
            jlos_tcp_socket_init(&socket1, self);
            socket1.remote_port = msg->src_port;
            socket1.remote_ip = srcIP_BE;
            socket1.local_port = msg->dst_port;
            socket1.local_ip = dstIP_BE;
            socket1.sequence_number = JLOS_SWAP_ENDIAN_32(msg->acknowledgement_number);
            socket1.acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->sequence_number) + 1;
            jlos_tcp_provider_send(self, &socket1, 0, 0, JLOS_TCP_RST);
            return true;
        }
    }
    if (socket && socket->state == JLOS_TCP_CLOSED) {
        jlos_tcp_key_t key = {socket->local_ip, socket->local_port};
        jlos_hash_node_t *node = jlos_hash_chain_find(&self->sockets, &key, tcp_match_closed_socket, socket);
        if (node) {
            jlos_hash_chain_remove(&self->sockets, node);
            self->num_sockets--;
            jlos_kfree(socket);
        }
    }
    return false;
}

void jlos_tcp_provider_send(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, uint8_t *data, uint16_t size, uint16_t flags)
{
    printk_debug("send %x%x->%x%x flags=%x size=%x%x seq=%x ack=%x\n",
        (JLOS_SWAP_ENDIAN_16(socket->local_port) >> 8) & 0xFF,
        JLOS_SWAP_ENDIAN_16(socket->local_port) & 0xFF,
        (JLOS_SWAP_ENDIAN_16(socket->remote_port) >> 8) & 0xFF,
        JLOS_SWAP_ENDIAN_16(socket->remote_port) & 0xFF,
        flags,
        (size >> 8) & 0xFF,
        size & 0xFF,
        socket->sequence_number,
        socket->acknowledgement_number);
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
    uint16_t length_incl_p_hdr = total_length + sizeof(jlos_tcp_pseudo_header_t);
    uint8_t *buffer = (uint8_t *)jlos_kalloc(length_incl_p_hdr);
    jlos_tcp_pseudo_header_t *phdr = (jlos_tcp_pseudo_header_t *)buffer;
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)(buffer + sizeof(jlos_tcp_pseudo_header_t));
    uint8_t *buffer2 = (uint8_t *)msg + (doff * 4);
    JLOS_TCP_SET_DATA_OFFSET_FLAGS(msg, doff, flags);
    msg->src_port = socket->local_port;
    msg->dst_port = socket->remote_port;
    msg->acknowledgement_number = JLOS_SWAP_ENDIAN_32(socket->acknowledgement_number);
    msg->sequence_number = JLOS_SWAP_ENDIAN_32(socket->sequence_number);
    msg->window_size = JLOS_SWAP_ENDIAN_16(0xFFFF);
    msg->urgent_ptr = 0;
    msg->options = ((flags & JLOS_TCP_SYN) != 0) ? JLOS_SWAP_ENDIAN_32(0x020405B4) : 0;
    socket->sequence_number += size;
    for (int i = 0; i < size; i++) {
        buffer2[i] = data[i];
    }
    phdr->src_ip = socket->local_ip;
    phdr->dst_ip = socket->remote_ip;
    phdr->protocol = 0x0600;
    phdr->total_length = JLOS_SWAP_ENDIAN_16(total_length);
    msg->checksum = 0;
    msg->checksum = jlos_internet_protocol_provider_check_sum((uint16_t *)buffer, length_incl_p_hdr);
    jlos_internet_protocol_handler_send(&self->base_handler, socket->remote_ip, (uint8_t *)msg, total_length);
    jlos_kfree(buffer);
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
        socket->sequence_number = 0xbeefcafe;
        jlos_tcp_key_t key = {socket->local_ip, socket->local_port};
        jlos_hash_chain_insert(&self->sockets, &key, &socket->hash_node);
        self->num_sockets++;
        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_SYN);
    }
    return socket;
}

void jlos_tcp_provider_disconnect(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket)
{
    socket->state = JLOS_TCP_FIN_WAIT1;
    jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_FIN | JLOS_TCP_ACK);
    socket->sequence_number++;
}

jlos_tcp_socket_t *jlos_tcp_provider_listen(jlos_tcp_provider_t* self, uint16_t port)
{
    jlos_tcp_socket_t *socket = (jlos_tcp_socket_t *)jlos_kalloc(sizeof(jlos_tcp_socket_t));
    if (socket) {
        jlos_tcp_socket_init(socket, self);
        socket->state = JLOS_TCP_LISTEN;
        socket->local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        socket->local_port = JLOS_SWAP_ENDIAN_16(port);
        jlos_tcp_key_t key = {socket->local_ip, socket->local_port};
        jlos_hash_chain_insert(&self->sockets, &key, &socket->hash_node);
        self->num_sockets++;
    }
    return socket;
}

void jlos_tcp_provider_bind(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, jlos_tcp_handler_t *handler)
{
    (void)self;
    socket->handler = handler;
}
