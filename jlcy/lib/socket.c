/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lib/socket.h>
#include <lib/syscall.h>

int32_t socket(int32_t domain, int32_t type, int32_t protocol)
{
    return (int32_t)syscall(SYSCALL_SOCKET, (uintptr_t)domain, (uintptr_t)type, (uintptr_t)protocol);
}

int32_t bind(int32_t fd, const sockaddr_t *addr, socklen_t addrlen)
{
    if (!addr || addr->sa_family != AF_INET || addrlen < sizeof(sockaddr_in_t)) {
        return -1;
    }
    const sockaddr_in_t *in = (const sockaddr_in_t *)addr;
    return (int32_t)syscall(SYSCALL_BIND, (uintptr_t)fd, (uintptr_t)in->sin_addr, (uintptr_t)ntohs(in->sin_port));
}

int32_t listen(int32_t fd, int32_t backlog)
{
    return (int32_t)syscall(SYSCALL_LISTEN, (uintptr_t)fd, (uintptr_t)backlog, 0);
}

int32_t accept(int32_t fd, sockaddr_t *addr, socklen_t *addrlen)
{
    return (int32_t)syscall(SYSCALL_ACCEPT, (uintptr_t)fd, (uintptr_t)addr, (uintptr_t)addrlen);
}

int32_t connect(int32_t fd, const sockaddr_t *addr, socklen_t addrlen)
{
    if (!addr || addr->sa_family != AF_INET || addrlen < sizeof(sockaddr_in_t)) {
        return -1;
    }
    const sockaddr_in_t *in = (const sockaddr_in_t *)addr;
    return (int32_t)syscall(SYSCALL_CONNECT, (uintptr_t)fd, (uintptr_t)in->sin_addr, (uintptr_t)ntohs(in->sin_port));
}

int32_t send(int32_t fd, const void *buf, uint32_t len, int32_t flags)
{
    if (flags != 0) {
        return -1;
    }
    return (int32_t)syscall(SYSCALL_SEND, (uintptr_t)fd, (uintptr_t)buf, (uintptr_t)len);
}

int32_t recv(int32_t fd, void *buf, uint32_t len, int32_t flags)
{
    if (flags != 0) {
        return -1;
    }
    return (int32_t)syscall(SYSCALL_RECV, (uintptr_t)fd, (uintptr_t)buf, (uintptr_t)len);
}
