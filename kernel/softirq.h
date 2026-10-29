/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_KERNEL_SOFTIRQ_H
#define _JLOS_KERNEL_SOFTIRQ_H

#include <common/types.h>

enum {
    JLOS_SOFTIRQ_NET_RX = 0,
    JLOS_SOFTIRQ_MAX,
};

typedef void (*jlos_softirq_fn_t)(void);

void jlos_softirq_register(uint32_t nr, jlos_softirq_fn_t fn);
void jlos_softirq_raise(uint32_t nr);
void jlos_softirq_run_pending(void);

#endif
