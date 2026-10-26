/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/net.h>

void jlos_hal_net_destroy(jlos_hal_net_dev_t *self)
{
    if (self && self->ops && self->ops->destroy) {
        self->ops->destroy(self);
    }
}

void jlos_hal_net_send(jlos_hal_net_dev_t *self, uint8_t *buffer, uint32_t size)
{
    if (self && self->ops && self->ops->send) {
        self->ops->send(self, buffer, size);
    }
}

uint64_t jlos_hal_net_get_mac_address(jlos_hal_net_dev_t *self)
{
    return self ? self->mac_address : 0;
}

void jlos_hal_net_set_ip_address(jlos_hal_net_dev_t *self, uint32_t ip)
{
    if (!self) {
        return;
    }
    self->ip_address = ip;
    if (self->ops && self->ops->set_ip_address) {
        self->ops->set_ip_address(self, ip);
    }
}

uint32_t jlos_hal_net_get_ip_address(jlos_hal_net_dev_t *self)
{
    return self ? self->ip_address : 0;
}

void jlos_hal_net_set_receive(jlos_hal_net_dev_t *self, jlos_hal_net_receive_fn_t on_receive, void *context)
{
    if (self) {
        self->on_receive = on_receive;
        self->receive_context = context;
    }
}

static jlos_hal_net_dev_t *s_primary_dev;

jlos_hal_net_dev_t *jlos_hal_net_primary_dev(void)
{
    return s_primary_dev;
}

void jlos_hal_net_set_primary_dev(jlos_hal_net_dev_t *dev)
{
    s_primary_dev = dev;
}