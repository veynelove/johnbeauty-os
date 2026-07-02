#include <net/udp.h>
#include <kernel/memory_manager.h>
#include <tools/config.h>
#include <kernel/printk.h>

void jlos_udp_handler_init(jlos_udp_handler_t* self)
{
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Handler initialized\n");
    #endif
    self->handle_udp_message = jlos_udp_handler_handle_udp_message;
}

void jlos_udp_handler_destroy(jlos_udp_handler_t* self)
{
}

void jlos_udp_handler_handle_udp_message(jlos_udp_handler_t* self, jlos_udp_socket_t* socket, uint8_t *m_data, uint16_t m_size)
{
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Handler received data, size=");
    printf_hex(m_size & 0xFF);
    printf_hex((m_size >> 8) & 0xFF);
    printf("\n");
    #endif
}

void jlos_udp_socket_init(jlos_udp_socket_t* self, jlos_udp_provider_t *backend)
{
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Socket initialized\n");
    #endif
    self->backend = backend;
    self->handler = NULL;
    self->m_listening = false;
    self->handle_udp_message = jlos_udp_socket_handle_udp_message;
    self->send = jlos_udp_socket_send;
    self->disconnect = jlos_udp_socket_disconnect;
}

void jlos_udp_socket_destroy(jlos_udp_socket_t* self)
{
}

void jlos_udp_socket_handle_udp_message(jlos_udp_socket_t* self, uint8_t *m_data, uint16_t m_size)
{
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Socket received data\n");
    #endif
    if (self->handler) {
        self->handler->handle_udp_message(self->handler, self, m_data, m_size);
    }
}

void jlos_udp_socket_send(jlos_udp_socket_t* self, uint8_t *m_data, uint16_t m_size)
{
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Socket sending data\n");
    #endif
    jlos_udp_provider_send(self->backend, self, m_data, m_size);
}

void jlos_udp_socket_disconnect(jlos_udp_socket_t* self)
{
    jlos_udp_provider_disconnect(self->backend, self);
}

void jlos_udp_provider_init(jlos_udp_provider_t* self, jlos_internet_protocol_provider_t *backend)
{
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Provider initializing...\n");
    #endif
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x11);
    self->base_handler.on_internet_protocol_received = (bool (*)(jlos_internet_protocol_handler_t*, uint32_t, uint32_t, uint8_t*, uint32_t))jlos_udp_provider_on_internet_protocol_received;
    self->m_num_sockets = 0;
    self->m_free_port = 1024;

    self->sockets = (jlos_udp_socket_t **)jlos_malloc(sizeof(jlos_udp_socket_t*) * JLOS_NET_MAX_SLOTS);
    for (int i = 0; i < JLOS_NET_MAX_SLOTS; i++) {
        self->sockets[i] = NULL;
    }
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Provider initialized\n");
    #endif
}

void jlos_udp_provider_destroy(jlos_udp_provider_t* self)
{
    jlos_free(self->sockets);
    self->sockets = NULL;
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

bool jlos_udp_provider_on_internet_protocol_received(jlos_udp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t m_size)
{
    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Received UDP packet\n");
    #endif
    
    if (m_size < sizeof(jlos_udp_header_t)) {
        #if KERNEL_CONFIG_DEBUG_NETWORK
        printf("UDP: Packet too small\n");
        #endif
        return false;
    }
    jlos_udp_header_t *msg = (jlos_udp_header_t *)internet_protocol_payload;

    #if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Destination port=");
    printf_hex(msg->m_dst_port & 0xFF);
    printf_hex((msg->m_dst_port >> 8) & 0xFF);
    printf("\n");
    #endif

    jlos_udp_socket_t *socket = NULL;
    for (uint16_t i = 0; i < self->m_num_sockets && socket == NULL; i++) {
        if (self->sockets[i]->m_local_port == msg->m_dst_port && self->sockets[i]->m_local_ip == dstIP_BE
            && self->sockets[i]->m_remote_port == msg->m_src_port && self->sockets[i]->m_remote_ip == srcIP_BE) {
            socket = self->sockets[i];
        }
    }
    for (uint16_t i = 0; i < self->m_num_sockets && socket == NULL; i++) {
        if (self->sockets[i]->m_local_port == msg->m_dst_port && self->sockets[i]->m_local_ip == dstIP_BE
            && self->sockets[i]->m_listening) {
            socket = self->sockets[i];
            socket->m_remote_port = msg->m_src_port;
            socket->m_remote_ip = srcIP_BE;
            #if KERNEL_CONFIG_DEBUG_NETWORK
            printf("UDP: Socket matched\n");
            #endif
        }
    }
    if (socket) {
        socket->handle_udp_message(socket, internet_protocol_payload + sizeof(jlos_udp_header_t), m_size - sizeof(jlos_udp_header_t));
    }
    return false;
}

jlos_udp_socket_t *jlos_udp_provider_connect(jlos_udp_provider_t* self, uint32_t ip, uint16_t port)
{
    jlos_udp_socket_t *socket = (jlos_udp_socket_t *)jlos_malloc(sizeof(jlos_udp_socket_t));
    if (socket) {
        jlos_udp_socket_init(socket, self);
        socket->m_remote_port = JLOS_SWAP_ENDIAN_16(port);
        socket->m_remote_ip = ip;
        socket->m_local_port = JLOS_SWAP_ENDIAN_16(self->m_free_port++);
        socket->m_local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        self->sockets[self->m_num_sockets++] = socket;
    }
    return socket;
}

jlos_udp_socket_t *jlos_udp_provider_listen(jlos_udp_provider_t* self, uint16_t port)
{
    jlos_udp_socket_t *socket = (jlos_udp_socket_t *)jlos_malloc(sizeof(jlos_udp_socket_t));
    if (socket) {
        jlos_udp_socket_init(socket, self);
        socket->m_listening = true;
        socket->m_local_port = JLOS_SWAP_ENDIAN_16(port);
        socket->m_local_ip = jlos_internet_protocol_provider_get_ip_address(self->base_handler.backend);
        self->sockets[self->m_num_sockets++] = socket;
    }
    return socket;
}

void jlos_udp_provider_disconnect(jlos_udp_provider_t* self, jlos_udp_socket_t *socket)
{
    for (uint16_t i = 0; i < self->m_num_sockets && socket != NULL; i++) {
        if (self->sockets[i] == socket) {
            self->sockets[i] = self->sockets[--self->m_num_sockets];
            jlos_free(socket);
            break;
        }
    }
}

void jlos_udp_provider_send(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, uint8_t *m_data, uint16_t m_size)
{
    uint16_t m_total_length = m_size + sizeof(jlos_udp_header_t);
    uint8_t *buffer = (uint8_t *)jlos_malloc(m_total_length);
    uint8_t *buffer2 = buffer + sizeof(jlos_udp_header_t);
    jlos_udp_header_t *msg = (jlos_udp_header_t *)buffer;
    msg->m_src_port = socket->m_local_port;
    msg->m_dst_port = socket->m_remote_port;
    msg->m_length = JLOS_SWAP_ENDIAN_16(m_total_length);
    for (int i = 0; i < m_size; i++) {
        buffer2[i] = m_data[i];
    }
    msg->m_checksum = 0;
    jlos_internet_protocol_handler_send(&self->base_handler, socket->m_remote_ip, buffer, m_total_length);
    jlos_free(buffer);
}

void jlos_udp_provider_bind(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, jlos_udp_handler_t *handler)
{
    socket->handler = handler;
}