/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_HAL_NET_H
#define _JLOS_HAL_NET_H

#include <common/types.h>

#define JLOS_HAL_NET_ADDR_LEN       6
#define JLOS_HAL_NET_PRIV_SIZE      64

typedef enum {
    JLOS_HAL_NET_DEV_AMD_AM79C973 = 1,
    JLOS_HAL_NET_DEV_VIRTIO_NET   = 2,
} jlos_hal_net_dev_type_t;

typedef struct jlos_hal_net_dev jlos_hal_net_dev_t;

typedef void (*jlos_hal_net_receive_fn_t)(void *context, uint8_t *buffer, uint32_t size);

typedef struct {
    void (*destroy)(jlos_hal_net_dev_t *self);
    void (*send)(jlos_hal_net_dev_t *self, uint8_t *buffer, uint32_t size);
    void (*set_ip_address)(jlos_hal_net_dev_t *self, uint32_t ip);
    void (*poll)(jlos_hal_net_dev_t *self);
} jlos_hal_net_ops_t;

typedef struct jlos_hal_net_dev {
    jlos_hal_net_dev_type_t     dev_type;
    uint64_t                    mac_address;
    uint32_t                    ip_address;
    const jlos_hal_net_ops_t    *ops;
    jlos_hal_net_receive_fn_t   on_receive;
    void                        *receive_context;
    uint8_t                     priv[JLOS_HAL_NET_PRIV_SIZE];
    uint8_t                     inited;
} jlos_hal_net_dev_t;

void jlos_hal_net_destroy(jlos_hal_net_dev_t *self);
void jlos_hal_net_send(jlos_hal_net_dev_t *self, uint8_t *buffer, uint32_t size);
uint64_t jlos_hal_net_get_mac_address(jlos_hal_net_dev_t *self);
void jlos_hal_net_set_ip_address(jlos_hal_net_dev_t *self, uint32_t ip);
uint32_t jlos_hal_net_get_ip_address(jlos_hal_net_dev_t *self);
void jlos_hal_net_set_receive(jlos_hal_net_dev_t *self, jlos_hal_net_receive_fn_t on_receive, void *context);

jlos_hal_net_dev_t *jlos_hal_net_primary_dev(void);
void jlos_hal_net_set_primary_dev(jlos_hal_net_dev_t *dev);

#endif