#include <net/udp.h>
#include <kernel/memory_manager.h>
#include <tools/config.h>

extern void printf(const char *str);
extern void printf_hex(uint8_t);

void jlos_udp_handler_init(jlos_udp_handler_t* self)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Handler initialized\n");
#endif
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
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Handler received data, size=");
    printf_hex(size & 0xFF);
    printf_hex((size >> 8) & 0xFF);
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
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Socket received data\n");
#endif
    if (self->handler) {
        self->handler->handle_udp_message(self->handler, self, data, size);
    }
}

void jlos_udp_socket_send(jlos_udp_socket_t* self, uint8_t *data, uint16_t size)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Socket sending data\n");
#endif
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

void jlos_udp_provider_init(jlos_udp_provider_t* self, jlos_internet_protocol_provider_t *backend)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Provider initializing...\n");
#endif
    jlos_internet_protocol_handler_init(&self->base_handler, backend, 0x11);
    self->base_handler.on_internet_protocol_received =
        (bool (*)(jlos_internet_protocol_handler_t*, uint32_t, uint32_t, uint8_t*, uint32_t))jlos_udp_provider_on_internet_protocol_received;
    self->num_sockets = 0;
    self->free_port = 1024;

    jlos_hash_chain_init(&self->sockets, JLOS_NET_HASH_CHAIN_NUM, udp_hash_ip_port, udp_cmp_ip_port);
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Provider initialized\n");
#endif
}

void jlos_udp_provider_destroy(jlos_udp_provider_t* self)
{
    jlos_hash_chain_destroy(&self->sockets);
    jlos_internet_protocol_handler_destroy(&self->base_handler);
}

static int udp_match_socket(jlos_hash_node_t *node, void *args1)
{
    jlos_udp_socket_t *socket = container_of(node, jlos_udp_socket_t, hash_node);
    uint32_t *args = args1;
    if (socket->local_ip != args[2] || socket->local_port != (uint16_t)args[3]) {
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

bool jlos_udp_provider_on_internet_protocol_received(jlos_udp_provider_t* self, uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t *internet_protocol_payload, uint32_t size)
{
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Received UDP packet\n");
#endif
    
    if (size < sizeof(jlos_udp_header_t)) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("UDP: Packet too small\n");
#endif
        return false;
    }
    jlos_udp_header_t *msg = (jlos_udp_header_t *)internet_protocol_payload;

#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("UDP: Destination port=");
    printf_hex(msg->dst_port & 0xFF);
    printf_hex((msg->dst_port >> 8) & 0xFF);
    printf("\n");
#endif

    jlos_udp_socket_t *socket = NULL;
    jlos_udp_key_t key = {dstIP_BE, msg->dst_port};
    uint32_t args[] = {srcIP_BE, msg->src_port, dstIP_BE, msg->dst_port};
    jlos_hash_node_t *node = jlos_hash_chain_find(&self->sockets, &key, udp_match_socket, args);
    if (node) {
        socket = container_of(node, jlos_udp_socket_t, hash_node);
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("UDP: Socket matched\n");
#endif
    }
    if (socket) {
        socket->handle_udp_message(socket, internet_protocol_payload + sizeof(jlos_udp_header_t), size - sizeof(jlos_udp_header_t));
        return true;
    }
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("UDP: Socket not matched\n");
#endif
    return false;
}

jlos_udp_socket_t *jlos_udp_provider_connect(jlos_udp_provider_t* self, uint32_t ip, uint16_t port)
{
    jlos_udp_socket_t *socket = (jlos_udp_socket_t *)jlos_malloc(sizeof(jlos_udp_socket_t));
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
    jlos_udp_socket_t *socket = (jlos_udp_socket_t *)jlos_malloc(sizeof(jlos_udp_socket_t));
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
    jlos_free(socket);
}

void jlos_udp_provider_send(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, uint8_t *data, uint16_t size)
{
    uint16_t total_length = size + sizeof(jlos_udp_header_t);
    uint8_t *buffer = (uint8_t *)jlos_malloc(total_length);
    uint8_t *buffer2 = buffer + sizeof(jlos_udp_header_t);
    jlos_udp_header_t *msg = (jlos_udp_header_t *)buffer;
    msg->src_port = socket->local_port;
    msg->dst_port = socket->remote_port;
    msg->length = JLOS_SWAP_ENDIAN_16(total_length);
    for (int i = 0; i < size; i++) {
        buffer2[i] = data[i];
    }
    msg->checksum = 0;
    jlos_internet_protocol_handler_send(&self->base_handler, socket->remote_ip, buffer, total_length);
    jlos_free(buffer);
}

void jlos_udp_provider_bind(jlos_udp_provider_t* self, jlos_udp_socket_t *socket, jlos_udp_handler_t *handler)
{
    (void)self;
    socket->handler = handler;
}