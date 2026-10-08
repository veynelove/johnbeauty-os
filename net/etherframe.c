/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <net/etherframe.h>
#include <kernel/memory_manager.h>
#include <include/config.h>

#define JLOS_KERNEL_LOG_SUBSYS "net"
#include <kernel/printk.h>

void jlos_ether_frame_handler_init(jlos_ether_frame_handler_t* self, jlos_ether_frame_provider_t *backend, uint16_t etherType_BE)
{
    self->etherType_BE = JLOS_SWAP_ENDIAN_16(etherType_BE);
    self->backend = backend;
    self->on_ether_frame_received = jlos_ether_frame_handler_on_ether_frame_received;
    jlos_hash_chain_insert(&backend->handlers, &self->etherType_BE, &self->hash_node);
}

void jlos_ether_frame_handler_destroy(jlos_ether_frame_handler_t* self)
{
    jlos_hash_chain_remove(&self->backend->handlers, &self->hash_node);
}

bool jlos_ether_frame_handler_on_ether_frame_received(jlos_ether_frame_handler_t* self, jlos_net_sk_buff_t *skb)
{
    (void)self;
    (void)skb;
    return false;
}

void jlos_ether_frame_handler_send(jlos_ether_frame_handler_t* self, uint64_t dstMAC_BE, uint16_t etherType_BE, jlos_net_sk_buff_t *skb)
{
    jlos_ether_frame_provider_send(self->backend, dstMAC_BE, etherType_BE, skb);
}

uint32_t jlos_ether_frame_handler_get_ip_address(jlos_ether_frame_handler_t* self)
{
    return jlos_ether_frame_provider_get_ip_address(self->backend);
}

static int ether_frame_cmp(const void *key, const void *node)
{
    uint16_t be = *(uint16_t *)key;
    jlos_ether_frame_handler_t *handler = container_of(node, jlos_ether_frame_handler_t, hash_node);
    return be - handler->etherType_BE;
}

static bool mac_address_eq(uint8_t *a, uint8_t *b)
{
    for (int i = 0; i < 6; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static bool mac_address_is_broadcast(uint8_t *mac)
{
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0xFF) return false;
    }
    return true;
}

static void uint64_to_mac(uint64_t mac_be, uint8_t *dest)
{
    for (int i = 0; i < 6; i++) {
        dest[i] = (mac_be >> (8 * i)) & 0xFF;
    }
}

static void jlos_ether_frame_provider_on_raw_data_received(jlos_ether_frame_provider_t* self, jlos_net_sk_buff_t *skb)
{
    if (jlos_net_skb_len(skb) < sizeof(jlos_ether_frame_header_t)) {
        printk_debug("frame too small\n");
        jlos_net_skb_free(skb);
        return;
    }
    jlos_ether_frame_header_t *frame = (jlos_ether_frame_header_t *)skb->data;
    uint8_t my_mac[6];
    uint64_to_mac(jlos_amd_am79c973_get_mac_address(self->base_handler.backend), my_mac);
    if (mac_address_is_broadcast(frame->dstMAC) || mac_address_eq(frame->dstMAC, my_mac)) {
        jlos_hash_node_t *node = jlos_hash_chain_see(&self->handlers, &frame->etherType_BE);
        if (node) {
            jlos_ether_frame_handler_t *handler = container_of(node, jlos_ether_frame_handler_t, hash_node);
            jlos_net_skb_pull(skb, sizeof(jlos_ether_frame_header_t));
            bool send_back = handler->on_ether_frame_received(handler, skb);
            if (send_back) {
                jlos_ether_frame_header_t *eh = (jlos_ether_frame_header_t *)jlos_net_skb_push(skb, sizeof(jlos_ether_frame_header_t));
                for (int i = 0; i < 6; i++) {
                    eh->dstMAC[i] = eh->srcMAC[i];
                }
                uint64_to_mac(jlos_amd_am79c973_get_mac_address(self->base_handler.backend), eh->srcMAC);
                jlos_amd_am79c973_send(self->base_handler.backend, skb->data, jlos_net_skb_len(skb));
            }
        }
        else {
            printk_debug("no handler for this type\n");
        }
    }
    else {
        printk_debug("frame not for us\n");
    }
    jlos_net_skb_free(skb);
}

void jlos_ether_frame_provider_init(jlos_ether_frame_provider_t* self, jlos_amd_am79c973_t *backend)
{
    jlos_rawdata_handler_init(&self->base_handler, backend);
    self->base_handler.on_raw_data_received = (void (*)(jlos_rawdata_handler_t *, jlos_net_sk_buff_t *))jlos_ether_frame_provider_on_raw_data_received;
    jlos_hash_chain_init(&self->handlers, JLOS_NET_HASH_CHAIN_NUM, jlos_hash_uint16, ether_frame_cmp);
}

void jlos_ether_frame_provider_destroy(jlos_ether_frame_provider_t* self)
{
    jlos_hash_chain_destroy(&self->handlers);
    jlos_rawdata_handler_destroy(&self->base_handler);
}

void jlos_ether_frame_provider_send(jlos_ether_frame_provider_t* self, uint64_t dstMAC_BE, uint16_t etherType_BE, jlos_net_sk_buff_t *skb)
{
    jlos_ether_frame_header_t *frame = (jlos_ether_frame_header_t *)jlos_net_skb_push(skb, sizeof(jlos_ether_frame_header_t));
    uint64_to_mac(dstMAC_BE, frame->dstMAC);
    uint64_to_mac(jlos_amd_am79c973_get_mac_address(self->base_handler.backend), frame->srcMAC);
    frame->etherType_BE = etherType_BE;
    jlos_amd_am79c973_send(self->base_handler.backend, skb->data, jlos_net_skb_len(skb));
    jlos_net_skb_free(skb);
}

uint64_t jlos_ether_frame_provider_get_mac_address(jlos_ether_frame_provider_t* self)
{
    return jlos_amd_am79c973_get_mac_address(self->base_handler.backend);
}

uint32_t jlos_ether_frame_provider_get_ip_address(jlos_ether_frame_provider_t* self)
{
    return jlos_amd_am79c973_get_ip_address(self->base_handler.backend);
}
