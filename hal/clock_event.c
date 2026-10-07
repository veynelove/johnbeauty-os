/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/clock_event.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "clocke"
#include <kernel/printk.h>

static JLOS_LIST_HEAD(s_evt_devs);
static jlos_clock_event_device_t *s_active_evt;

void jlos_clock_event_register(jlos_clock_event_device_t *dev)
{
    if (!dev) {
        return;
    }
    jlos_list_add(&dev->list, &s_evt_devs);
}

jlos_clock_event_device_t *jlos_clock_event_get_active(void)
{
    return s_active_evt;
}

void jlos_clock_event_start_periodic(uint32_t hz)
{
    if (!s_active_evt || !s_active_evt->set_state_periodic) {
        return;
    }
    s_active_evt->set_state_periodic(hz);
}

void jlos_clock_event_shutdown(void)
{
    if (!s_active_evt || !s_active_evt->set_state_shutdown) {
        return;
    }
    s_active_evt->set_state_shutdown();
}

static void clock_event_select_and_start(void)
{
    jlos_clock_event_device_t *best = NULL;
    jlos_clock_event_device_t *d;
    jlos_list_for_each_entry(d, &s_evt_devs, list) {
        if (!best || d->rating > best->rating) {
            best = d;
        }
    }
    s_active_evt = best;
    if (best) {
        printk_info("selected = %s, rating = %u\n", best->name, best->rating);
    } else {
        printk_err("no clock event device registered\n");
    }
    jlos_clock_event_start_periodic(JLOS_HAL_TIME_FREQ_HZ);
}

void jlos_clock_event_start_oneshot(void)
{
    if (!s_active_evt || !s_active_evt->set_state_oneshot) {
        return;
    }
    s_active_evt->set_state_oneshot();
}

void jlos_clock_event_set_next(uint64_t delta_ns)
{
    if (!s_active_evt || !s_active_evt->set_next_event) {
        return;
    }
    s_active_evt->set_next_event(delta_ns);
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, clock_event_select_and_start);
