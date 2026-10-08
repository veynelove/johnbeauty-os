/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <net/dns.h>
#include <net/network.h>
#include <kernel/timek.h>
#include <hal/hal_arch.h>
#include <include/config.h>

#define JLOS_KERNEL_SUBSYS "dns"
#include <kernel/printk.h>

static uint8_t *dns_encode_name(uint8_t *p, const char * name)
{
    while (*name) {
        const char *dot = name;
        while (*dot && *dot != '.') {
            dot++;
        }
        uint8_t len = (uint8_t)(dot - name);
        *p++ = len;
        for (uint8_t i = 0; i < len; i++) {
            *p++ = (uint8_t)name[i];
        }
        if (*dot) {
            dot++;
        }
        name = dot;
    }
    *p++ = 0;
    return p;
}

static const uint8_t *dns_skip_name(const uint8_t *p, const uint8_t *end)
{
    while (p < end && *p) {
        if ((*p & 0xC0) == 0xC0) {
            if (p + 1 >= end) {
                return NULL;
            }
            return p + 2;
        }
        p += *p + 1;
    }
    if (p >= end) {
        return NULL;
    }
    return p + 1;
}

static void jlos_dns_on_response(jlos_udp_handler_t *handler, jlos_udp_socket_t *socket, uint8_t *data, uint16_t size)
{
    jlos_dns_resolver_t *self = container_of(handler, jlos_dns_resolver_t, handler);
    (void)socket;
    if (size < sizeof(jlos_dns_header_t)) {
        return;
    }
    jlos_dns_header_t *header = (jlos_dns_header_t *)data;
    if (header->id != self->query_id) {
        return;
    }
    uint16_t flags = JLOS_SWAP_ENDIAN_16(header->flags);
    if (!(flags & 0x8000)) {
        return;
    }
    uint16_t ancount = JLOS_SWAP_ENDIAN_16(header->ancount);
    if (ancount == 0) {
        return;
    }
    const uint8_t *p = data + sizeof(jlos_dns_header_t);
    const uint8_t *end = data + size;

    uint16_t qdcount = JLOS_SWAP_ENDIAN_16(header->qdcount);
    for (uint16_t i = 0; i < qdcount; i++) {
        p = dns_skip_name(p, end);
        if (!p || p + 4 > end) {
            return;
        }
        p += 4;
    }
    for (uint16_t i = 0; i < ancount; i++) {
        p = dns_skip_name(p, end);
        if (!p || p + 10 > end) {
            return;
        }
        uint16_t type = JLOS_SWAP_ENDIAN_16(*(uint16_t *)p);
        p += 2;
        p += 2;
        p += 4;
        uint16_t rdlength = JLOS_SWAP_ENDIAN_16(*(uint16_t *)p);
        p += 2;
        if (p + rdlength > end) {
            return;
        }
        if (type == JLOS_DNS_TYPE_A && rdlength == 4) {
            jlos_memcpy(&self->resolved_ip, p, 4);
            self->resolved = true;
            printk_info("resolved, ip = %x\n", self->resolved_ip);
            return;
        }
        p += rdlength;
    }
}

void jlos_dns_resolver_init(jlos_dns_resolver_t *self, jlos_udp_provider_t *udp)
{
    self->handler.handle_udp_message = jlos_dns_on_response;
    self->udp = udp;
    self->socket = NULL;
    self->query_id = 0;
    self->resolved_ip = 0;
    self->resolved = false;
}

bool jlos_dns_resolve(jlos_dns_resolver_t *self, const char *name, uint32_t *out_ip)
{
    if (!g_network_stack || g_network_stack->config.dns_server == 0) {
        return false;
    }
    self->socket = jlos_udp_provider_connect(self->udp, g_network_stack->config.dns_server, JLOS_DNS_SERVER_PORT);
    if (!self->socket) {
        return false;
    }
    jlos_udp_provider_bind(self->udp, self->socket, &self->handler);
    self->resolved = false;
    self->resolved_ip = 0;
    self->query_id = (uint16_t)jlos_timek_get_monotonic_ns();
    uint8_t buffer[JLOS_DNS_MAX_PACKET];
    jlos_dns_header_t *header = (jlos_dns_header_t *)buffer;
    header->id = self->query_id;
    header->flags = JLOS_SWAP_ENDIAN_16(JLOS_DNS_FLAG_RD);
    header->qdcount = JLOS_SWAP_ENDIAN_16(1);
    header->ancount = 0;
    header->nscount = 0;
    header->arcount = 0;

    uint8_t *p = buffer + sizeof(jlos_dns_header_t);
    p = dns_encode_name(p, name);
    *(uint16_t *)p = JLOS_SWAP_ENDIAN_16(JLOS_DNS_TYPE_A);
    p += 2;
    *(uint16_t *)p = JLOS_SWAP_ENDIAN_16(JLOS_DNS_CLASS_IN);
    p += 2;

    uint16_t total = (uint16_t)(p - buffer);
    self->socket->send(self->socket, buffer, total);
    printk_info("query sent, name = %s, id = %x\n", name, self->query_id);

    uint32_t start = jlos_timek_get_monotonic_ns();
    while (!self->resolved) {
        if (jlos_timek_get_monotonic_ns() - start > JLOS_DNS_TIMEOUT_NS) {
            jlos_udp_provider_disconnect(self->udp, self->socket);
            self->socket = NULL;
            printk_warn("resolve timeout, name = %s\n", name);
            return false;
        }
        jlos_hal_cpu_relax();
    }
    *out_ip = self->resolved_ip;
    jlos_udp_provider_disconnect(self->udp, self->socket);
    self->socket = NULL;
    return true;
}
