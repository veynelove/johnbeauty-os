/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLCY_NETINET_IN_H
#define _JLCY_NETINET_IN_H

#include <include/types.h>

#define INADDR_ANY             0
#define INADDR_BROADCAST       0xFFFFFFFF
#define INADDR_LOOPBACK        0x7F000001

typedef struct sockaddr_in {
    uint16_t    sin_family;
    uint16_t    sin_port;
    uint32_t    sin_addr;
    uint8_t     sin_zero[8];
} __attribute__((packed)) sockaddr_in_t;

#define htons(x)    __builtin_bswap16(x)
#define htonl(x)    __builtin_bswap32(x)
#define ntohs(x)    __builtin_bswap16(x)
#define ntohl(x)    __builtin_bswap32(x)

#endif
