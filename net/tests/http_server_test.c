/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <net/network.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_http"
#include <kernel/printk.h>

JLOS_TEST(http, server_listen)
{
    /* 无网络栈（无网卡）时 g_network_stack 为 nullptr，不可解引用 */
    if (!g_network_stack) {
        printk_info("skip: no network stack\n");
        return;
    }
    jlos_tcp_provider_t *tcp = &g_network_stack->tcp;
    jlos_tcp_socket_t *tcpsocket = jlos_tcp_provider_listen(tcp, 1234);
    JLOS_TEST_NOT_NULL(tcpsocket);
    jlos_tcp_provider_disconnect(tcp, tcpsocket);
}
