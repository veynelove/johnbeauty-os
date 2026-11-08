/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/hrtimer.h>
#include <kernel/initcall.h>
#include <kernel/timek.h>
#include <hal/clock_event.h>
#include <hal/smp.h>
#include <kernel/printk.h>

DEFINE_PER_CPU(jlos_hrtimer_base_t, jlos_cpu_hrtimer_base);

static int hrtimer_cmp(const jlos_rbtree_node_t *a, const jlos_rbtree_node_t *b)
{
    jlos_hrtimer_t *ta = jlos_rbtree_entry(a, jlos_hrtimer_t, node);
    jlos_hrtimer_t *tb = jlos_rbtree_entry(b, jlos_hrtimer_t, node);
    if (ta->expires < tb->expires) {
        return -1;
    }
    if (ta->expires > tb->expires) {
        return 1;
    }
    return 0;
}

void jlos_hrtimer_base_init(jlos_hrtimer_base_t *base)
{
    jlos_rbtree_init(&base->active);
    jlos_spinlock_init(&base->lock);
}

void jlos_hrtimer_init(jlos_hrtimer_t *timer, jlos_hrtimer_func_t func, void *data)
{
    timer->func = func;
    timer->data = data;
    timer->expires = 0;
    timer->interval = 0;
    timer->active = false;
    timer->base = nullptr;
}

void jlos_hrtimer_start(jlos_hrtimer_base_t *base, jlos_hrtimer_t *timer, uint64_t time_ns, uint64_t interval_ns, jlos_hrtimer_mode_t mode)
{
    if (timer->active && timer->base != base) {
        jlos_hrtimer_base_t *old = timer->base;
        jlos_hrtimer_base_t *first = ((uintptr_t)old < (uintptr_t)base) ? old : base;
        jlos_hrtimer_base_t *second = ((uintptr_t)old < (uintptr_t)base) ? base : old;
        uint32_t first_flags = jlos_spin_lock_irqsave(&first->lock);
        jlos_spin_lock(&second->lock);
        jlos_rbtree_remove(&old->active, &timer->node);
        timer->active = false;
        jlos_spin_unlock(&second->lock);
        jlos_spin_unlock_irqrestore(&first->lock, first_flags);
    }
    uint32_t fl = jlos_spin_lock_irqsave(&base->lock);
    if (timer->active) {
        jlos_rbtree_remove(&base->active, &timer->node);
    }
    uint64_t now = jlos_timek_get_monotonic_ns();
    timer->expires = (mode == JLOS_HRTIMER_MODE_ABS) ? time_ns : now + time_ns;
    timer->interval = interval_ns;
    timer->active = true;
    timer->base = base;
    jlos_rbtree_insert(&base->active, &timer->node, hrtimer_cmp);
    jlos_spin_unlock_irqrestore(&base->lock, fl);
}

void jlos_hrtimer_cancel(jlos_hrtimer_base_t *base, jlos_hrtimer_t *timer)
{
    jlos_hrtimer_base_t *tbase = (timer->base) ? timer->base : base;
    uint32_t flags = jlos_spin_lock_irqsave(&tbase->lock);
    if (timer->active) {
        jlos_rbtree_remove(&tbase->active, &timer->node);
        timer->active = false;
    }
    jlos_spin_unlock_irqrestore(&tbase->lock, flags);
}

jlos_hrtimer_base_t *jlos_hrtimer_get_base(void)
{
    return this_cpu_ptr(jlos_cpu_hrtimer_base);
}

void jlos_hrtimer_interrupt(void)
{
    jlos_hrtimer_base_t *base = jlos_hrtimer_get_base();
    uint64_t now = jlos_timek_get_monotonic_ns();
    for (;;) {
        uint32_t flags = jlos_spin_lock_irqsave(&base->lock);
        jlos_rbtree_node_t *node = jlos_rbtree_first(&base->active);
        jlos_hrtimer_t *timer = nullptr;
        if (node) {
            timer = jlos_rbtree_entry(node, jlos_hrtimer_t, node);
            if (timer->expires <= now) {
                jlos_rbtree_remove(&base->active, &timer->node);
                timer->active = false;
            } else {
                timer = nullptr;
            }
        }
        if (!timer) {
            jlos_spin_unlock_irqrestore(&base->lock, flags);
            break;
        }
        if (timer->interval > 0) {
            do {
                timer->expires += timer->interval;
            } while (timer->expires <= now);
            timer->active = true;
            jlos_rbtree_insert(&base->active, &timer->node, hrtimer_cmp);
        }
        jlos_spin_unlock_irqrestore(&base->lock, flags);
        timer->func(timer);
        now = jlos_timek_get_monotonic_ns();
    }
    uint32_t flags = jlos_spin_lock_irqsave(&base->lock);
    jlos_rbtree_node_t *node = jlos_rbtree_first(&base->active);
    if (node) {
        jlos_hrtimer_t *timer = jlos_rbtree_entry(node, jlos_hrtimer_t, node);
        if (timer->expires > now) {
            jlos_clock_event_set_next(timer->expires - now);
        } else {
            jlos_clock_event_set_next(0);
        }
    }
    jlos_spin_unlock_irqrestore(&base->lock, flags);
}

void jlos_hrtimer_base_init_cpu(void)
{
    jlos_hrtimer_base_t *base = jlos_hrtimer_get_base();
    jlos_rbtree_init(&base->active);
    jlos_spinlock_init(&base->lock);
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_hrtimer_base_init_cpu);
