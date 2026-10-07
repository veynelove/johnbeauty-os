/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_NET_SKBUFF_H
#define _JLOS_NET_SKBUFF_H

#include <common/types.h>
#include <dsa/list.h>
#include <hal/atomic.h>

#define JLOS_NET_SKB_DATA_THRESH    2048

#define JLOS_NET_SKB_DATA_KALLOC    0
#define JLOS_NET_SKB_DATA_PAGES     1

#define JLOS_NET_SKB_LAYER_NONE     0
#define JLOS_NET_SKB_LAYER_ETHER    1
#define JLOS_NET_SKB_LAYER_IPV4     2
#define JLOS_NET_SKB_LAYER_UDP      3
#define JLOS_NET_SKB_LAYER_TCP      4
#define JLOS_NET_SKB_LAYER_ICMP     5

#define JLOS_NET_SKB_HEAD_ALIGN     16

typedef struct {
    uint8_t             *head;
    uint8_t             *data;
    uint8_t             *tail;
    uint8_t             *end;
    uint32_t            data_len;
    uint8_t             data_flags;
    uint8_t             data_pages;
    uint32_t            src_ip;
    uint32_t            dst_ip;
    uint16_t            src_port;
    uint16_t            dst_port;
    uint8_t             protocol;
    uint8_t             layer;
    jlos_list_head_t    list;
    jlos_atomic_t       refcount;
} jlos_net_sk_buff_t;

void jlos_net_skb_init(void);

jlos_net_sk_buff_t *jlos_net_skb_alloc(uint32_t size);
void jlos_net_skb_free(jlos_net_sk_buff_t *skb);

jlos_net_sk_buff_t *jlos_net_skb_get(jlos_net_sk_buff_t *skb);

void jlos_net_skb_reserve(jlos_net_sk_buff_t *skb, uint32_t len);
uint8_t *jlos_net_skb_push(jlos_net_sk_buff_t *skb, uint32_t len);
uint8_t *jlos_net_skb_pull(jlos_net_sk_buff_t *skb, uint32_t len);
uint8_t *jlos_net_skb_put(jlos_net_sk_buff_t *skb, uint32_t len);

static inline uint32_t jlos_net_skb_len(const jlos_net_sk_buff_t *skb)
{
    return skb->tail - skb->data;
}

static inline uint32_t jlos_net_skb_headroom(const jlos_net_sk_buff_t *skb)
{
    return skb->data - skb->head;
}

static inline uint32_t jlos_net_skb_tailroom(const jlos_net_sk_buff_t *skb)
{
    return skb->end - skb->tail;
}

#endif
