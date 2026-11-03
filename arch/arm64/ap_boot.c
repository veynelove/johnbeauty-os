/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/gic.h>
#include <arch/arm64/smp.h>
#include <hal/clock_event.h>
#include <kernel/hrtimer.h>
#include <kernel/multitask.h>
#include <kernel/timek.h>

#define JLOS_KERNEL_LOG_SUBSYS "smp"
#include <kernel/printk.h>

extern void jlos_arch_timer_cpu_init(void);

void jlos_ap_main(void)
{
    uint32_t cpu = jlos_hal_get_cpu_id();


    jlos_arch_gic_cpu_init();
    jlos_arch_timer_cpu_init();

    jlos_clock_event_select();
    jlos_hrtimer_base_init_cpu();
    jlos_tick_init_cpu();
    jlos_sched_init_cpu();

    *this_cpu_ptr(jlos_cpu_online) = 1;
    printk_info("AP %u online\n", cpu);

    __asm__ __volatile__("msr daifclr, #2");
    printk_info("ap %u idle ready\n", cpu);

    for (;;) {
        __asm__ __volatile__("wfi");
    }
}
