/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_NET_DNS_H
#define _JLOS_NET_DNS_H

#include <net/udp.h>

#define JLOS_DNS_SERVER_PORT    53

#define JLOS_DNS_TYPE_A         1
#define JLOS_DNS_CLASS_IN       1

#define JLOS_DNS_FLAG_RD        0x0100

#define JLOS_DNS_MAX_PACKET     512
#define JLOS_DNS_TIMEOUT_NS     2000000000

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) jlos_dns_header_t;

typedef struct {
    jlos_udp_handler_t  handler;
    jlos_udp_socket_t   *socket;
    jlos_udp_provider_t *udp;
    uint16_t            query_id;
    uint32_t            resolved_ip;
    bool                resolved;
} jlos_dns_resolver_t;

void jlos_dns_resolver_init(jlos_dns_resolver_t *self, jlos_udp_provider_t *udp);
bool jlos_dns_resolve(jlos_dns_resolver_t *self, const char *name, uint32_t *out_ip);

#endif
