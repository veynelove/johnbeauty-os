/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <kernel/memory_manager.h>
#include <net/network.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_udp"
#include <kernel/printk.h>

typedef struct {
    jlos_udp_handler_t base;
} udp_handler_t;

static void udp_handler_handle_udp_message(jlos_udp_handler_t *self, jlos_udp_socket_t *socket, uint8_t *data, uint16_t size)
{
    (void)self;
    printk_debug("udp received: %u bytes\n", size);
    socket->send(socket, data, size);
}

JLOS_TEST(udp, server_listen)
{
    jlos_udp_provider_t *udp = &g_network_stack->udp;
    udp_handler_t *udphandler = jlos_kalloc(sizeof(udp_handler_t));
    JLOS_ASSERT_NOT_NULL(udphandler);
    jlos_udp_handler_init(&udphandler->base);
    udphandler->base.handle_udp_message = udp_handler_handle_udp_message;
    jlos_udp_socket_t *udpsocket = jlos_udp_provider_listen(udp, 5678);
    JLOS_TEST_NOT_NULL(udpsocket);
    jlos_udp_provider_bind(udp, udpsocket, &udphandler->base);
}
