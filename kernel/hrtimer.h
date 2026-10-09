/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_KERNEL_HRTIMER_H
#define _JLOS_KERNEL_HRTIMER_H

#include <common/types.h>
#include <dsa/rbtree.h>
#include <hal/spinlock.h>

typedef enum {
    JLOS_HRTIMER_MODE_REL,
    JLOS_HRTIMER_MODE_ABS,
} jlos_hrtimer_mode_t;

typedef struct jlos_hrtimer jlos_hrtimer_t;

typedef int (*jlos_hrtimer_func_t)(jlos_hrtimer_t *timer);

struct jlos_hrtimer {
    jlos_rbtree_node_t  node;
    uint64_t            expires;
    uint64_t            interval;
    jlos_hrtimer_func_t func;
    void                *data;
    bool                active;
};

struct jlos_task;

typedef struct jlos_hrtimer_sleeper {
    jlos_hrtimer_t      timer;
    struct jlos_task    *task;
} jlos_hrtimer_sleeper_t;

typedef struct {
    jlos_rbtree_t   active;
    jlos_spinlock_t lock;
} jlos_hrtimer_base_t;

void jlos_hrtimer_base_init(jlos_hrtimer_base_t *base);
void jlos_hrtimer_init(jlos_hrtimer_t *timer, jlos_hrtimer_func_t func, void *data);
void jlos_hrtimer_start(jlos_hrtimer_base_t *base, jlos_hrtimer_t *timer, uint64_t time_ns, uint64_t interval_ns, jlos_hrtimer_mode_t mode);
void jlos_hrtimer_cancel(jlos_hrtimer_base_t *base, jlos_hrtimer_t *timer);

jlos_hrtimer_base_t *jlos_hrtimer_get_base(void);
void jlos_hrtimer_base_init_cpu(void);
void jlos_hrtimer_interrupt(void);

#endif
