/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/clocksource.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "clocks"
#include <kernel/printk.h>

static JLOS_LIST_HEAD(s_cs_devs);
static jlos_clocksource_t *s_active_cs;

void jlos_clocksource_register(jlos_clocksource_t *cs)
{
    if (!cs) {
        return;
    }
    jlos_list_add(&cs->list, &s_cs_devs);
}

jlos_clocksource_t *jlos_clocksource_get_active(void)
{
    return s_active_cs;
}

uint64_t jlos_clocksource_read_cycles(void)
{
    if (!s_active_cs || !s_active_cs->read) {
        return 0;
    }
    return s_active_cs->read();
}

uint64_t jlos_clocksource_cycles_to_ns(uint64_t delta)
{
    if (!s_active_cs) {
        return 0;
    }
    return (delta * s_active_cs->mult) >> s_active_cs->shift;
}

static void clocksource_select(void)
{
    jlos_clocksource_t *best = NULL;
    jlos_clocksource_t *d;
    jlos_list_for_each_entry(d, &s_cs_devs, list) {
        if (!best || d->rating > best->rating) {
            best = d;
        }
    }
    s_active_cs = best;
    if (best) {
        printk_info("selected = %s, rating = %u\n", best->name, best->rating);
    } else {
        printk_err("no clocksource registered\n");
    }
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, clocksource_select);
