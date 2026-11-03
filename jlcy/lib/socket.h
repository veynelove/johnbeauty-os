/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLCY_LIB_SOCKET_H
#define _JLCY_LIB_SOCKET_H

#include <include/types.h>
#include <include/in.h>

#define AF_INET              2
#define SOCK_STREAM          1
#define SOCK_DGRAM           2

typedef uint32_t socklen_t;

typedef struct sockaddr {
    uint16_t    sa_family;
    uint8_t     sa_data[14];
} sockaddr_t;

int32_t socket(int32_t domain, int32_t type, int32_t protocol);
int32_t bind(int32_t fd, const sockaddr_t *addr, socklen_t addrlen);
int32_t listen(int32_t fd, int32_t backlog);
int32_t accept(int32_t fd, sockaddr_t *addr, socklen_t *addrlen);
int32_t connect(int32_t fd, const sockaddr_t *addr, socklen_t addrlen);
int32_t send(int32_t fd, const void *buf, uint32_t len, int32_t flags);
int32_t recv(int32_t fd, void *buf, uint32_t len, int32_t flags);

#endif
