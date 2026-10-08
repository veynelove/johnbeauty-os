/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_NET_ICMP_H
#define _JLOS_NET_ICMP_H

#include <net/ipv4.h>

typedef struct {
    uint8_t     type;
    uint8_t     code;
    uint16_t    check_sum;
    uint32_t    data;
} __attribute__((packed)) jlos_icmp_message_t;

typedef struct jlos_icmp jlos_icmp_t;

struct jlos_icmp {
    jlos_internet_protocol_handler_t base_handler;
};

void jlos_icmp_init(jlos_icmp_t* self, jlos_internet_protocol_provider_t *backend);
void jlos_icmp_destroy(jlos_icmp_t* self);
void jlos_icmp_request_echo_reply(jlos_icmp_t* self, uint32_t ip_be);

#endif