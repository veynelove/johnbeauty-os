#include <tools/tests/udp_server_te.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"

typedef struct {
    jlos_udp_handler_t base;
} udp_handler_t;

static void udp_handler_handle_udp_message(jlos_udp_handler_t* self, jlos_udp_socket_t* socket, uint8_t *data, uint16_t size)
{
    (void)self;
    printk_debug("udp received: %u bytes\n", size);
    socket->send(socket, data, size);
}

void udp_server_test(jlos_udp_provider_t *udp)
{
    udp_handler_t *udphandler = jlos_kalloc(sizeof(udp_handler_t));
    jlos_udp_handler_init(&udphandler->base);
    udphandler->base.handle_udp_message = udp_handler_handle_udp_message;
    jlos_udp_socket_t *udpsocket = jlos_udp_provider_listen(udp, 5678);
    jlos_udp_provider_bind(udp, udpsocket, &udphandler->base);
    printk_info("udp server listening on port 5678\n");
}