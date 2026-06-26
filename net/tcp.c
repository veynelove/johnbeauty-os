#include <net/tcp.h>
#include <kernel/memory_manager.h>

void jlos_tcp_handler_init(jlos_tcp_handler_t* self)
{
    self->handle_tcp_message = jlos_tcp_handler_handle_tcp_message;
}

void jlos_tcp_handler_destroy(jlos_tcp_handler_t* self)
{
}

bool jlos_tcp_handler_handle_tcp_message(jlos_tcp_handler_t* self, jlos_tcp_socket_t* socket, uint8_t *m_data, uint16_t m_size)
{
    return true;
}

void jlos_tcp_socket_init(jlos_tcp_socket_t* self, jlos_tcp_provider_t *backend)
{
    self->backend = backend;
    self->handler = NULL;
    self->m_state = JLOS_TCP_CLOSED;
    self->handle_tcp_message = jlos_tcp_socket_handle_tcp_message;
    self->send = jlos_tcp_socket_send;
    self->disconnect = jlos_tcp_socket_disconnect;
}

void jlos_tcp_socket_destroy(jlos_tcp_socket_t* self)
{
}

bool jlos_tcp_socket_handle_tcp_message(jlos_tcp_socket_t* self, uint8_t *m_data, uint16_t m_size)
{
    if (self->handler) {
        return self->handler->handle_tcp_message(self->handler, self, m_data, m_size);
    }
    return false;
}

void jlos_tcp_socket_send(jlos_tcp_socket_t* self, uint8_t *m_data, uint16_t m_size)
{
    while (self->m_state != JLOS_TCP_ESTABLISHED) {}
    jlos_tcp_provider_send(self->backend, self, m_data, m_size, (JLOS_TCP_PSH | JLOS_TCP_ACK));
}

void jlos_tcp_socket_disconnect(jlos_tcp_socket_t* self)
{
    jlos_tcp_provider_disconnect(self->backend, self);
}

void jlos_tcp_provider_init(jlos_tcp_provider_t* self, jlos_internet_protocol_provider_t *backend)
{
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x06);
    self->base_handler.on_internet_protocol_received = (bool (*)(jlos_internet_protocol_handler_t*, uint32_t, uint32_t, uint8_t*, uint32_t))jlos_tcp_provider_on_internet_protocol_received;
    self->m_num_sockets = 0;
    self->m_free_port = 1024;
    
    for (int i = 0; i < 65535; i++) {
        self->sockets[i] = NULL;
    }
}

void jlos_tcp_provider_destroy(jlos_tcp_provider_t* self)
{
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

bool jlos_tcp_provider_on_internet_protocol_received(jlos_tcp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size)
{
    if (m_size < 20) {
        return false;
    }
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)internet_protocol_payload;

    jlos_tcp_socket_t *socket = NULL;
    for (uint16_t i = 0; i < self->m_num_sockets && socket == NULL; i++) {
        if (self->sockets[i]->m_local_port == msg->m_dst_port && self->sockets[i]->m_local_ip == dstIP_BE
            && self->sockets[i]->m_state == JLOS_TCP_LISTEN && ((msg->m_flags) & (JLOS_TCP_SYN | JLOS_TCP_ACK)) == JLOS_TCP_SYN) {
            socket = self->sockets[i];
        }
        else if (self->sockets[i]->m_local_port == msg->m_dst_port && self->sockets[i]->m_local_ip == dstIP_BE
            && self->sockets[i]->m_remote_port == msg->m_src_port && self->sockets[i]->m_remote_ip == srcIP_BE) {
            socket = self->sockets[i];
        }
    }

    bool reset = false;
    if (socket && (msg->m_flags & JLOS_TCP_RST)) {
        socket->m_state = JLOS_TCP_CLOSED;
    }
    if (socket && socket->m_state != JLOS_TCP_CLOSED) {
        switch ((msg->m_flags) & (JLOS_TCP_SYN | JLOS_TCP_ACK | JLOS_TCP_FIN)) {
            case JLOS_TCP_SYN:
                if (socket->m_state == JLOS_TCP_LISTEN) {
                    socket->m_state = JLOS_TCP_SYN_RECEIVED;
                    socket->m_remote_port = msg->m_src_port;
                    socket->m_remote_ip = srcIP_BE;
                    socket->m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
                    socket->m_sequence_number = 0xbeefcafe;
                    jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_SYN | JLOS_TCP_ACK));
                    socket->m_sequence_number++;
                } else {
                    reset = true;
                }
                break;
            case (JLOS_TCP_SYN | JLOS_TCP_ACK):
                if (socket->m_state == JLOS_TCP_SYN_SENT) {
                    socket->m_state = JLOS_TCP_ESTABLISHED;
                    socket->m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
                    socket->m_sequence_number++;
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
                switch (socket->m_state) {
                    case JLOS_TCP_ESTABLISHED:
                        socket->m_state = JLOS_TCP_CLOSE_WAIT;
                        socket->m_acknowledgement_number++;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_FIN | JLOS_TCP_ACK));
                        break;
                    case JLOS_TCP_CLOSE_WAIT:
                        socket->m_state = JLOS_TCP_CLOSED;
                        break;
                    case JLOS_TCP_FIN_WAIT1:
                    case JLOS_TCP_FIN_WAIT2:
                        socket->m_state = JLOS_TCP_CLOSED;
                        socket->m_acknowledgement_number++;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        break;
                    default:
                        reset = true;
                }
                break;
            case JLOS_TCP_ACK:
                switch (socket->m_state) {
                    case JLOS_TCP_SYN_RECEIVED:
                        socket->m_state = JLOS_TCP_ESTABLISHED;
                        return false;
                    case JLOS_TCP_FIN_WAIT1:
                        socket->m_state = JLOS_TCP_FIN_WAIT2;
                        return false;
                    case JLOS_TCP_CLOSE_WAIT:
                        socket->m_state = JLOS_TCP_CLOSED;
                        break;
                }
                break;
            default:
                if (JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) == socket->m_acknowledgement_number) {
                    reset = !socket->handle_tcp_message(socket, (internet_protocol_payload + msg->header_size32 * 4), (m_size - msg->header_size32 * 4));
                    if (!reset) {
                        socket->m_acknowledgement_number += (m_size - msg->header_size32 * 4);
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                    }
                } else {
                    reset = true;
                }
        }
    }
    if (reset) {
        if (socket) {
            jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_RST);
        } else {
            jlos_tcp_socket_t socket1;
            jlos_tcp_socket_init(&socket1, self);
            socket1.m_remote_port = msg->m_src_port;
            socket1.m_remote_ip = srcIP_BE;
            socket1.m_local_port = msg->m_dst_port;
            socket1.m_local_ip = dstIP_BE;
            socket1.m_sequence_number = JLOS_SWAP_ENDIAN_32(msg->m_acknowledgement_number);
            socket1.m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
            jlos_tcp_provider_send(self, &socket1, 0, 0, JLOS_TCP_RST);
            return true;
        }
    }
    if (socket && socket->m_state == JLOS_TCP_CLOSED) {
        for (uint16_t i = 0; i < self->m_num_sockets && socket != NULL; i++) {
            if (self->sockets[i] == socket) {
                self->sockets[i] = self->sockets[--self->m_num_sockets];
                jlos_free(socket);
                break;
            }
        }
    }
    return false;
}

void jlos_tcp_provider_send(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, uint8_t *m_data, uint16_t m_size, uint16_t m_flags)
{
    uint16_t m_total_length = m_size + sizeof(jlos_tcp_header_t);
    uint16_t length_incl_p_hdr = m_total_length + sizeof(jlos_tcp_pseudo_header_t);

    uint8_t *buffer = (uint8_t *)jlos_malloc(length_incl_p_hdr);
    
    jlos_tcp_pseudo_header_t *phdr = (jlos_tcp_pseudo_header_t *)buffer;
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)(buffer + sizeof(jlos_tcp_pseudo_header_t));
    
    uint8_t *buffer2 = buffer + sizeof(jlos_tcp_header_t) + sizeof(jlos_tcp_pseudo_header_t);

    msg->header_size32 = sizeof(jlos_tcp_header_t) / 4;
    msg->m_src_port = socket->m_local_port;
    msg->m_dst_port = socket->m_remote_port;
    msg->m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(socket->m_acknowledgement_number);
    msg->m_sequence_number = JLOS_SWAP_ENDIAN_32(socket->m_sequence_number);
    msg->m_reserved = 0;
    msg->m_flags = m_flags;
    msg->m_window_size = 0xFFFF;
    msg->m_urgent_ptr = 0;
    msg->m_options = ((m_flags & JLOS_TCP_SYN) != 0) ? 0xB4050402 : 0;
    
    socket->m_sequence_number += m_size;

    for (int i = 0; i < m_size; i++) {
        buffer2[i] = m_data[i];
    }
    phdr->m_src_ip = socket->m_local_ip;
    phdr->m_dst_ip = socket->m_remote_ip;
    phdr->m_protocol = 0x0600;
    phdr->m_total_length = JLOS_SWAP_ENDIAN_16(m_total_length);
    msg->m_checksum = 0;
    msg->m_checksum = jlos_internet_protocol_provider_check_sum((uint16_t *)buffer, length_incl_p_hdr);
    jlos_internet_protocol_handler_send(&self->base_handler, socket->m_remote_ip, (uint8_t *)msg, m_total_length);
    jlos_free(buffer);
}

jlos_tcp_socket_t *jlos_tcp_provider_connect(jlos_tcp_provider_t* self, uint32_t ip, uint16_t port)
{
    jlos_tcp_socket_t *socket = (jlos_tcp_socket_t *)jlos_malloc(sizeof(jlos_tcp_socket_t));
    if (socket) {
        jlos_tcp_socket_init(socket, self);
        socket->m_remote_port = JLOS_SWAP_ENDIAN_16(port);
        socket->m_remote_ip = ip;
        socket->m_local_port = JLOS_SWAP_ENDIAN_16(self->m_free_port++);
        socket->m_local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        self->sockets[self->m_num_sockets++] = socket;
        socket->m_state = JLOS_TCP_SYN_SENT;
        socket->m_sequence_number = 0xbeefcafe;
        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_SYN);
    }
    return socket;
}

void jlos_tcp_provider_disconnect(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket)
{
    socket->m_state = JLOS_TCP_FIN_WAIT1;
    jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_FIN | JLOS_TCP_ACK);
    socket->m_sequence_number++;
}

jlos_tcp_socket_t *jlos_tcp_provider_listen(jlos_tcp_provider_t* self, uint16_t port)
{
    jlos_tcp_socket_t *socket = (jlos_tcp_socket_t *)jlos_malloc(sizeof(jlos_tcp_socket_t));
    if (socket) {
        jlos_tcp_socket_init(socket, self);
        socket->m_state = JLOS_TCP_LISTEN;
        socket->m_local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        socket->m_local_port = JLOS_SWAP_ENDIAN_16(port);
        self->sockets[self->m_num_sockets++] = socket;
    }
    return socket;
}

void jlos_tcp_provider_bind(jlos_tcp_provider_t* self, jlos_tcp_socket_t *socket, jlos_tcp_handler_t *handler)
{
    socket->handler = handler;
}