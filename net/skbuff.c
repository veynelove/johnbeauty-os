/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <net/skbuff.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <hal/paging.h>

static jlos_memory_slab_cache_t *skbuff_head_cache;

void jlos_net_skb_init(void)
{
    skbuff_head_cache =
        jlos_memory_slab_cache_create("skbuff_head", sizeof(jlos_net_sk_buff_t), JLOS_NET_SKB_HEAD_ALIGN, 0, NULL, NULL);
}

static uint8_t *jlos_net_skb_data_alloc(uint32_t size, uint8_t *flags, uint8_t *pages)
{
    if (size <= JLOS_NET_SKB_DATA_THRESH) {
        *flags = JLOS_NET_SKB_DATA_KALLOC;
        *pages = 0;
        return jlos_kalloc(size);
    }
    uint32_t npages = JLOS_EXCEPT_CEIL(size, JLOS_PAGE_SIZE);
    *flags = JLOS_NET_SKB_DATA_PAGES;
    *pages = (uint8_t)npages;
    return jlos_page_frame_alloc_n(npages);
}

static void jlos_net_skb_data_free(uint8_t *head, uint8_t flags, uint8_t pages)
{
    if (flags == JLOS_NET_SKB_DATA_KALLOC) {
        jlos_kfree(head);
    } else {
        jlos_page_frame_free_n(head, pages);
    }
}

jlos_net_sk_buff_t *jlos_net_skb_alloc(uint32_t size)
{
    jlos_net_sk_buff_t *skb = jlos_memory_slab_cache_alloc(skbuff_head_cache);
    if (!skb) {
        return NULL;
    }
    uint8_t flags;
    uint8_t pages;
    uint8_t *head = jlos_net_skb_data_alloc(size, &flags, &pages);
    if (!head) {
        jlos_memory_slab_cache_free(skbuff_head_cache, skb);
        return NULL;
    }
    skb->head = head;
    skb->data = head;
    skb->tail = head;
    skb->end = head + size;
    skb->data_len = size;
    skb->data_flags = flags;
    skb->data_pages = pages;
    skb->src_ip = 0;
    skb->dst_ip = 0;
    skb->src_port = 0;
    skb->dst_port = 0;
    skb->protocol = 0;
    skb->layer = JLOS_NET_SKB_LAYER_NONE;
    jlos_list_init(&skb->list);
    jlos_atomic_set(&skb->refcount, 1);
    return skb;
}

void jlos_net_skb_free(jlos_net_sk_buff_t *skb)
{
    if (!skb) {
        return;
    }
    if (jlos_atomic_dec_return(&skb->refcount) > 0) {
        return;
    }
    jlos_net_skb_data_free(skb->head, skb->data_flags, skb->data_pages);
    jlos_memory_slab_cache_free(skbuff_head_cache, skb);
}

jlos_net_sk_buff_t *jlos_net_skb_get(jlos_net_sk_buff_t *skb)
{
    jlos_atomic_inc(&skb->refcount);
    return skb;
}

void jlos_net_skb_reserve(jlos_net_sk_buff_t *skb, uint32_t len)
{
    skb->data += len;
    skb->tail += len;
}

uint8_t *jlos_net_skb_push(jlos_net_sk_buff_t *skb, uint32_t len)
{
    skb->data -= len;
    return skb->data;
}

uint8_t *jlos_net_skb_pull(jlos_net_sk_buff_t *skb, uint32_t len)
{
    skb->data += len;
    return skb->data;
}

uint8_t *jlos_net_skb_put(jlos_net_sk_buff_t *skb, uint32_t len)
{
    uint8_t *tmp = skb->tail;
    skb->tail += len;
    return tmp;
}
