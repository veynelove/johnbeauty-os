/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lib/socket.h>
#include <lib/syscall.h>

#define JLOS_SOCKET_ECHO_EXIT_MAGIC    7
#define JLOS_SOCKET_ECHO_FAIL_MAGIC    8
#define JLOS_SOCKET_ECHO_PORT          9000
#define JLOS_SOCKET_ECHO_MSG           "ping-pong-echo"

static int32_t socket_echo_client(int32_t server_fd_inherited)
{
    (void)server_fd_inherited;
    int32_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    sockaddr_in_t addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(JLOS_SOCKET_ECHO_PORT);
    addr.sin_addr = INADDR_LOOPBACK;
    for (uint32_t i = 0; i < sizeof(addr.sin_zero); i++) {
        addr.sin_zero[i] = 0;
    }
    if (connect(fd, (const sockaddr_t *)&addr, sizeof(addr)) != 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    int32_t n = send(fd, JLOS_SOCKET_ECHO_MSG, sizeof(JLOS_SOCKET_ECHO_MSG) - 1, 0);
    if (n <= 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    char buf[64];
    int32_t got = recv(fd, buf, sizeof(buf) - 1, 0);
    if (got != n) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    buf[got] = '\0';
    for (int32_t i = 0; i < got; i++) {
        if (buf[i] != JLOS_SOCKET_ECHO_MSG[i]) {
            printf("client verify mismatch at %d\n", i);
            return JLOS_SOCKET_ECHO_FAIL_MAGIC;
        }
    }
    task_fd_close(fd);
    return JLOS_SOCKET_ECHO_EXIT_MAGIC;
}

int main(void)
{
    int32_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    sockaddr_in_t addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(JLOS_SOCKET_ECHO_PORT);
    addr.sin_addr = INADDR_LOOPBACK;
    for (uint32_t i = 0; i < sizeof(addr.sin_zero); i++) {
        addr.sin_zero[i] = 0;
    }
    if (bind(server_fd, (const sockaddr_t *)&addr, sizeof(addr)) != 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    if (listen(server_fd, 1) != 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    int32_t child_pid = fork();
    if (child_pid == 0) {
        return socket_echo_client(server_fd);
    }
    if (child_pid < 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    int32_t conn = accept(server_fd, nullptr, nullptr);
    if (conn < 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    char buf[64];
    int32_t n = recv(conn, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    if (send(conn, buf, (uint32_t)n, 0) != n) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    task_fd_close(conn);
    int32_t code = 0;
    if (wait_pid((uint32_t)child_pid, &code) != child_pid || code != JLOS_SOCKET_ECHO_EXIT_MAGIC) {
        return JLOS_SOCKET_ECHO_FAIL_MAGIC;
    }
    task_fd_close(server_fd);
    return 42;
}