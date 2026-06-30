#include <net/tcp.h>
#include <kernel/memory_manager.h>

extern void printf(const char *str);
extern void printf_hex(uint8_t);
extern void printf_hex32(uint32_t);

static const char *tcp_state_str(uint8_t state)
{
    switch (state) {
        case JLOS_TCP_CLOSED: return "CLOSED";
        case JLOS_TCP_LISTEN: return "LISTEN";
        case JLOS_TCP_SYN_SENT: return "SYN_SENT";
        case JLOS_TCP_SYN_RECEIVED: return "SYN_RCVD";
        case JLOS_TCP_ESTABLISHED: return "ESTAB";
        case JLOS_TCP_FIN_WAIT1: return "FIN_W1";
        case JLOS_TCP_FIN_WAIT2: return "FIN_W2";
        case JLOS_TCP_CLOSE_WAIT: return "CL_WAIT";
        default: return "?";
    }
}

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

    self->sockets = (jlos_tcp_socket_t **)jlos_malloc(sizeof(jlos_tcp_socket_t*) * JLOS_NET_MAX_SLOTS);
    for (int i = 0; i < JLOS_NET_MAX_SLOTS; i++) {
        self->sockets[i] = NULL;
    }
}

void jlos_tcp_provider_destroy(jlos_tcp_provider_t* self)
{
    jlos_free(self->sockets);
    self->sockets = NULL;
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

bool jlos_tcp_provider_on_internet_protocol_received(jlos_tcp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size)
{
    if (m_size < 20) {
        return false;
    }
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)internet_protocol_payload;
    uint16_t flags = JLOS_TCP_GET_FLAGS(msg);
    uint8_t data_offset = JLOS_TCP_GET_DATA_OFFSET(msg);

#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("TCP: ");
    printf_hex((JLOS_SWAP_ENDIAN_16(msg->m_src_port) >> 8) & 0xFF);
    printf_hex(JLOS_SWAP_ENDIAN_16(msg->m_src_port) & 0xFF);
    printf("->");
    printf_hex((JLOS_SWAP_ENDIAN_16(msg->m_dst_port) >> 8) & 0xFF);
    printf_hex(JLOS_SWAP_ENDIAN_16(msg->m_dst_port) & 0xFF);
    printf(" flags=");
    if (flags & JLOS_TCP_FIN) printf("F");
    if (flags & JLOS_TCP_SYN) printf("S");
    if (flags & JLOS_TCP_RST) printf("R");
    if (flags & JLOS_TCP_PSH) printf("P");
    if (flags & JLOS_TCP_ACK) printf("A");
    if (flags & JLOS_TCP_URG) printf("U");
    printf(" seq=");
    printf_hex32(JLOS_SWAP_ENDIAN_32(msg->m_sequence_number));
    printf(" ack=");
    printf_hex32(JLOS_SWAP_ENDIAN_32(msg->m_acknowledgement_number));
    printf("\n");
#endif

    jlos_tcp_socket_t *socket = NULL;
    for (uint16_t i = 0; i < self->m_num_sockets && socket == NULL; i++) {
        if (self->sockets[i]->m_local_port == msg->m_dst_port && self->sockets[i]->m_local_ip == dstIP_BE
            && self->sockets[i]->m_state == JLOS_TCP_LISTEN && (flags & (JLOS_TCP_SYN | JLOS_TCP_ACK)) == JLOS_TCP_SYN) {
            socket = self->sockets[i];
        }
        else if (self->sockets[i]->m_local_port == msg->m_dst_port && self->sockets[i]->m_local_ip == dstIP_BE
            && self->sockets[i]->m_remote_port == msg->m_src_port && self->sockets[i]->m_remote_ip == srcIP_BE) {
            socket = self->sockets[i];
        }
    }

    bool reset = false;
    if (socket && (flags & JLOS_TCP_RST)) {
        socket->m_state = JLOS_TCP_CLOSED;
    }
    if (socket && socket->m_state != JLOS_TCP_CLOSED) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("TCP: socket state=0x");
        printf_hex(socket->m_state);
        printf(" remote=");
        printf_hex((socket->m_remote_ip >> 0) & 0xFF);
        printf_hex((socket->m_remote_ip >> 8) & 0xFF);
        printf_hex((socket->m_remote_ip >> 16) & 0xFF);
        printf_hex((socket->m_remote_ip >> 24) & 0xFF);
        printf(":");
        {
            uint16_t rp = JLOS_SWAP_ENDIAN_16(socket->m_remote_port);
            printf_hex((rp >> 8) & 0xFF);
            printf_hex(rp & 0xFF);
        }
        printf("\n");
#endif
        switch (flags & (JLOS_TCP_SYN | JLOS_TCP_ACK | JLOS_TCP_FIN)) {
            case JLOS_TCP_SYN:
                if (socket->m_state == JLOS_TCP_LISTEN) {
                    socket->m_state = JLOS_TCP_SYN_RECEIVED;
                    socket->m_remote_port = msg->m_src_port;
                    socket->m_remote_ip = srcIP_BE;
                    socket->m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
                    socket->m_sequence_number = 0xbeefcafe;
#if KERNEL_CONFIG_DEBUG_NETWORK
                    printf("TCP: LISTEN -> SYN_RCVD, sending SYN-ACK\n");
#endif
                    jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_SYN | JLOS_TCP_ACK));
                    socket->m_sequence_number++;
                } else if (socket->m_state == JLOS_TCP_SYN_RECEIVED) {
#if KERNEL_CONFIG_DEBUG_NETWORK
                    printf("TCP: SYN_RCVD retransmitting SYN-ACK (lost?)\n");
#endif
                    jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_SYN | JLOS_TCP_ACK));
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
                        socket->m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                        jlos_tcp_provider_send(self, socket, 0, 0, (JLOS_TCP_FIN | JLOS_TCP_ACK));
                        break;
                    case JLOS_TCP_CLOSE_WAIT:
                        socket->m_state = JLOS_TCP_CLOSED;
                        break;
                    case JLOS_TCP_FIN_WAIT1:
                    case JLOS_TCP_FIN_WAIT2:
                        socket->m_state = JLOS_TCP_CLOSED;
                        socket->m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) + 1;
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
#if KERNEL_CONFIG_DEBUG_NETWORK
                        printf("TCP: 3-way handshake complete, state=ESTABLISHED\n");
#endif
                        return false;
                    case JLOS_TCP_FIN_WAIT1:
                        socket->m_state = JLOS_TCP_FIN_WAIT2;
                        return false;
                    case JLOS_TCP_CLOSE_WAIT:
                        socket->m_state = JLOS_TCP_CLOSED;
                        break;
                }
            default:
                if (JLOS_SWAP_ENDIAN_32(msg->m_sequence_number) == socket->m_acknowledgement_number) {
                    uint32_t header_bytes = data_offset * 4;
                    uint32_t payload_len = m_size - header_bytes;
#if KERNEL_CONFIG_DEBUG_NETWORK
                    printf("TCP: payload len=");
                    printf_hex((payload_len >> 8) & 0xFF);
                    printf_hex(payload_len & 0xFF);
                    printf(" bytes, calling handler\n");
#endif
                    reset = !socket->handle_tcp_message(socket, (internet_protocol_payload + header_bytes), payload_len);
                    if (!reset) {
                        socket->m_acknowledgement_number += payload_len;
                        jlos_tcp_provider_send(self, socket, 0, 0, JLOS_TCP_ACK);
                    }
                } else {
                    reset = true;
                }
        }
    }
    if (reset) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("TCP: sending RST (reset)\n");
#endif
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
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("TCP_SEND: ");
    printf_hex((JLOS_SWAP_ENDIAN_16(socket->m_local_port) >> 8) & 0xFF);
    printf_hex(JLOS_SWAP_ENDIAN_16(socket->m_local_port) & 0xFF);
    printf("->");
    printf_hex((JLOS_SWAP_ENDIAN_16(socket->m_remote_port) >> 8) & 0xFF);
    printf_hex(JLOS_SWAP_ENDIAN_16(socket->m_remote_port) & 0xFF);
    printf(" flags=");
    if (m_flags & JLOS_TCP_FIN) printf("F");
    if (m_flags & JLOS_TCP_SYN) printf("S");
    if (m_flags & JLOS_TCP_RST) printf("R");
    if (m_flags & JLOS_TCP_PSH) printf("P");
    if (m_flags & JLOS_TCP_ACK) printf("A");
    if (m_flags & JLOS_TCP_URG) printf("U");
    printf(" size=");
    printf_hex((m_size >> 8) & 0xFF);
    printf_hex(m_size & 0xFF);
    printf(" seq=");
    printf_hex32(socket->m_sequence_number);
    printf(" ack=");
    printf_hex32(socket->m_acknowledgement_number);
    printf("\n");
    if (m_size > 0 && m_data) {
        uint16_t dump = m_size > 40 ? 40 : m_size;
        printf("TCP_DATA: [HEX] ");
        for (uint16_t i = 0; i < dump; i++) { printf_hex(m_data[i]); printf(" "); }
        printf("\nTCP_DATA: [ASC] ");
        char foo[2] = " ";
        for (uint16_t i = 0; i < dump; i++) {
            uint8_t c = m_data[i];
            if (c >= 32 && c < 127) { foo[0] = c; printf(foo); }
            else if (c == 0x0D) printf(".");
            else if (c == 0x0A) printf("\\n\n        ");
            else printf(".");
        }
        printf("\n");
    }
#endif

    uint8_t doff;
    uint16_t tcp_hdr_len;
    if ((m_flags & JLOS_TCP_SYN) != 0) {
        doff = 6;
        tcp_hdr_len = 24;
    } else {
        doff = 5;
        tcp_hdr_len = 20;
    }

    uint16_t m_total_length = m_size + tcp_hdr_len;
    uint16_t length_incl_p_hdr = m_total_length + sizeof(jlos_tcp_pseudo_header_t);

    uint8_t *buffer = (uint8_t *)jlos_malloc(length_incl_p_hdr);
    
    jlos_tcp_pseudo_header_t *phdr = (jlos_tcp_pseudo_header_t *)buffer;
    jlos_tcp_header_t *msg = (jlos_tcp_header_t *)(buffer + sizeof(jlos_tcp_pseudo_header_t));
    
    uint8_t *buffer2 = (uint8_t *)msg + (doff * 4);

    JLOS_TCP_SET_DATA_OFFSET_FLAGS(msg, doff, m_flags);
    msg->m_src_port = socket->m_local_port;
    msg->m_dst_port = socket->m_remote_port;
    msg->m_acknowledgement_number = JLOS_SWAP_ENDIAN_32(socket->m_acknowledgement_number);
    msg->m_sequence_number = JLOS_SWAP_ENDIAN_32(socket->m_sequence_number);
    msg->m_window_size = JLOS_SWAP_ENDIAN_16(0xFFFF);
    msg->m_urgent_ptr = 0;
    msg->m_options = ((m_flags & JLOS_TCP_SYN) != 0) ? JLOS_SWAP_ENDIAN_32(0x020405B4) : 0;
    
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
#if KERNEL_CONFIG_DEBUG_NETWORK
    if (m_size > 0) {
        uint8_t *__p = (uint8_t *)&msg->m_data_offset_flags;
        printf("TCP_HDR: doff=");
        printf_hex(doff);
        printf(" tcp_hdr_len=");
        printf_hex((tcp_hdr_len>>8)&0xFF);
        printf_hex(tcp_hdr_len&0xFF);
        printf(" doff_flags=");
        printf_hex(__p[0]);
        printf(" ");
        printf_hex(__p[1]);
        printf(" -> ");
        printf_hex(((__p[0]>>4)&0xF));
        printf("\n");
        printf("TCP_PAY: [BUF2] ");
        uint16_t dmp = m_size > 8 ? 8 : m_size;
        for (uint16_t ii = 0; ii < dmp; ii++) { printf_hex(buffer2[ii]); printf(" "); }
        printf("\n");
        printf("FINAL_SEND: [MSG+0..29] ");
        uint8_t *__pp = (uint8_t *)msg;
        for (uint8_t jj = 0; jj < 30; jj++) { printf_hex(__pp[jj]); printf(" "); }
        printf("\n");
        printf("FINAL_SEND: bytes20..27 (payload start) = ");
        for (uint8_t jj = 20; jj < 28; jj++) { printf_hex(__pp[jj]); printf(" "); }
        printf("\n");
    }
#endif
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