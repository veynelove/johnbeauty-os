/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/softirq.h>
#include <kernel/initcall.h>
#include <hal/smp.h>
#include <hal/atomic.h>

#define JLOS_KERNEL_LOG_SUBSYS "softirq"
#include <kernel/printk.h>

DEFINE_PER_CPU(jlos_atomic_t, jlos_softirq_pending);

static jlos_softirq_fn_t s_softirq_handlers[JLOS_SOFTIRQ_MAX];

static void jlos_softirq_init(void)
{
    for (uint32_t cpu = 0; cpu < JLOS_MAX_CPUS; cpu++) {
        jlos_atomic_set(per_cpu_ptr_cpu(jlos_softirq_pending, cpu), 0);
    }
}

void jlos_softirq_register(uint32_t nr, jlos_softirq_fn_t fn)
{
    if (nr < JLOS_SOFTIRQ_MAX) {
        s_softirq_handlers[nr] = fn;
    }
}

void jlos_softirq_raise(uint32_t nr)
{
    if (nr >= JLOS_SOFTIRQ_MAX) {
        return;
    }
    jlos_atomic_t *pending = this_cpu_ptr(jlos_softirq_pending);
    int32_t old = jlos_atomic_read(pending);
    while (!jlos_atomic_cmpxchg(pending, &old, old | (1 << nr))) {
    }
}

void jlos_softirq_run_pending(void)
{
    jlos_atomic_t *pending = this_cpu_ptr(jlos_softirq_pending);
    int32_t mask = jlos_atomic_xchg(pending, 0);
    for (uint32_t nr = 0; nr < JLOS_SOFTIRQ_MAX; nr++) {
        if ((mask & (1 << nr)) && s_softirq_handlers[nr]) {
            s_softirq_handlers[nr]();
        }
    }
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_softirq_init);
