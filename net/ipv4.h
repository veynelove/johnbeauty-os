/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_NET_IPV4_H
#define _JLOS_NET_IPV4_H

#include <net/etherframe.h>
#include <net/arp.h>

#define JLOS_EPHEMERAL_PORT_START 1024

typedef struct {
    uint8_t     version_ihl;
    uint8_t     tos;
    uint16_t    total_length;
    uint16_t    ident;
    uint16_t    flags_and_offset;
    uint8_t     time_to_live;
    uint8_t     protocol;
    uint16_t    checksum;
    uint32_t    src_ip;
    uint32_t    dst_ip;
} __attribute__((packed)) jlos_ipv4_message_t;

#define JLOS_IPV4_GET_VERSION(msg) (((msg)->version_ihl >> 4) & 0x0F)
#define JLOS_IPV4_GET_IHL(msg)     ((msg)->version_ihl & 0x0F)
#define JLOS_IPV4_SET_VERSION_IHL(msg, ver, ihl) ((msg)->version_ihl = (((ver) & 0x0F) << 4) | ((ihl) & 0x0F))

#define JLOS_IPV4_BROADCAST 0xFFFFFFFF

#define JLOS_IPV4_FMT(ip)   ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF

typedef struct jlos_internet_protocol_provider jlos_internet_protocol_provider_t;

typedef struct jlos_internet_protocol_handler {
    jlos_internet_protocol_provider_t   *backend;
    uint8_t                             ip_protocol;
    bool (*on_internet_protocol_received)(struct jlos_internet_protocol_handler* self, jlos_net_sk_buff_t *skb);
} jlos_internet_protocol_handler_t;

struct jlos_internet_protocol_provider {
    jlos_ether_frame_handler_t          base_handler;
    jlos_internet_protocol_handler_t    *handlers[255];
    jlos_arp_t                          *arp;
    uint32_t                            gateway_ip;
    uint32_t                            subnet_mask;
};

void jlos_internet_protocol_handler_init(jlos_internet_protocol_handler_t* self, jlos_internet_protocol_provider_t *backend, uint8_t protocol);
void jlos_internet_protocol_handler_destroy(jlos_internet_protocol_handler_t* self);
bool jlos_internet_protocol_handler_on_internet_protocol_received(jlos_internet_protocol_handler_t* self, jlos_net_sk_buff_t *skb);
void jlos_internet_protocol_handler_send(jlos_internet_protocol_handler_t* self, uint32_t dstIP_BE, jlos_net_sk_buff_t *skb);

void jlos_internet_protocol_provider_init(jlos_internet_protocol_provider_t* self, jlos_ether_frame_provider_t *backend, jlos_arp_t *arp, uint32_t gateway_ip, uint32_t subnet_mask);
void jlos_internet_protocol_provider_destroy(jlos_internet_protocol_provider_t* self);
void jlos_internet_protocol_provider_send(jlos_internet_protocol_provider_t* self, uint32_t dstIP_BE, uint8_t protocol, jlos_net_sk_buff_t *skb);
uint32_t jlos_internet_protocol_provider_csum_partial(const void *data, uint32_t length_in_bytes, uint32_t init_csum);
uint16_t jlos_internet_protocol_provider_csum_fold(uint32_t csum);
uint32_t jlos_internet_protocol_provider_get_ip_address(jlos_internet_protocol_provider_t* self);

#endif