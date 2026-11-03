/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/gic.h>
#include <arch/arm64/interrupts.h>
#include <hal/clocksource.h>
#include <hal/clock_event.h>
#include <hal/irq.h>

#include <common/hardware/fdt.h>
#include <kernel/hrtimer.h>
#include <kernel/initcall.h>
#include <kernel/multitask.h>

#define JLOS_KERNEL_LOG_SUBSYS "timer"
#include <kernel/printk.h>

#define JLOS_ARCH_TIMER_PPI_EL1_PHYS   14
#define JLOS_ARCH_TIMER_CS_SHIFT       10
#define JLOS_ARCH_TIMER_NS_PER_SEC     1000000000ULL

#define JLOS_ARCH_TIMER_CTL_ENABLE     (1U << 0)
#define JLOS_ARCH_TIMER_CTL_IMASK      (1U << 1)

static uint32_t          s_timer_freq;
static uint32_t          s_timer_intid;
static uint64_t          s_period_ticks;
static jlos_irq_handler_t s_timer_irq;


static uint64_t timer_cntpct_read(void)
{
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static void timer_write_cval(uint64_t val)
{
    __asm__ __volatile__("msr cntp_cval_el0, %0" : : "r"(val));
}

static uint64_t timer_read_cval(void)
{
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntp_cval_el0" : "=r"(val));
    return val;
}

static void timer_write_ctl(uint32_t ctl)
{
    __asm__ __volatile__("msr cntp_ctl_el0, %0" : : "r"((uint64_t)ctl));
}

static uint64_t timer_cs_read(void)
{
    return timer_cntpct_read();
}

static jlos_clocksource_t s_timer_cs = {
    .name   = "arch_timer",
    .rating = 350,
    .read   = timer_cs_read,
    .mask   = 0xFFFFFFFFFFFFFFFFULL,
};

static void timer_set_state_periodic(uint32_t hz)
{
    s_period_ticks = s_timer_freq / hz;
    timer_write_cval(timer_cntpct_read() + s_period_ticks);
    timer_write_ctl(JLOS_ARCH_TIMER_CTL_ENABLE);
    jlos_arch_gic_irq_unmask(s_timer_intid);
}

static void timer_set_state_oneshot(void)
{
    s_period_ticks = 0;
    timer_write_ctl(0);
    jlos_arch_gic_irq_unmask(s_timer_intid);
}

static void timer_set_state_shutdown(void)
{
    s_period_ticks = 0;
    timer_write_ctl(0);
    jlos_arch_gic_irq_mask(s_timer_intid);
}

static void timer_set_next_event(uint64_t delta_ns)
{
    uint64_t ticks = (delta_ns * s_timer_freq) / JLOS_ARCH_TIMER_NS_PER_SEC;
    if (ticks == 0) {
        ticks = 1;
    }
    timer_write_cval(timer_cntpct_read() + ticks);
    timer_write_ctl(JLOS_ARCH_TIMER_CTL_ENABLE);
}

static jlos_clock_event_device_t s_timer_evt = {
    .name               = "arch_timer",
    .rating             = 400,
    .features           = JLOS_CLOCK_EVT_FEAT_PERIODIC | JLOS_CLOCK_EVT_FEAT_ONESHOT,
    .set_state_periodic = timer_set_state_periodic,
    .set_state_oneshot  = timer_set_state_oneshot,
    .set_state_shutdown = timer_set_state_shutdown,
    .set_next_event     = timer_set_next_event,
};

static uintptr_t timer_handle_irq(jlos_irq_handler_t *self, uintptr_t frame)
{

    jlos_irq_manager_t *mgr = self->interrupt_manager;
    jlos_hrtimer_interrupt();
    if (mgr && mgr->task_manager) {
        jlos_task_manager_tick_and_schedule(mgr->task_manager);
    }
    if (s_period_ticks) {
        uint64_t cval = timer_read_cval() + s_period_ticks;
        uint64_t now = timer_cntpct_read();
        if ((int64_t)(cval - now) <= 0) {
            cval = now + s_period_ticks;
        }
        timer_write_cval(cval);
    }
    return frame;
}

static void timer_intid_probe(void)
{
    s_timer_intid = 16 + JLOS_ARCH_TIMER_PPI_EL1_PHYS;
    int32_t node = jlos_fdt_find_node("/timer");
    if (node < 0) {
        return;
    }
    uint32_t len = 0;
    const uint32_t *intr = (const uint32_t *)jlos_fdt_get_prop(node, "interrupts", &len);
    if (intr && len >= 6 * JLOS_FDT_TOKEN_SIZE &&
        jlos_fdt_read_u32(&intr[3]) == 1) {    /* 1 = PPI */
        s_timer_intid = 16 + jlos_fdt_read_u32(&intr[4]);
    }
}

static void jlos_arch_timer_init(void)
{
    if (!jlos_arch_gic_ready()) {
        printk_err("gic not ready, timer disabled\n");
        return;
    }
    timer_intid_probe();

    uint64_t freq;
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(freq));
    if (freq != 0) {
        s_timer_freq = (uint32_t)freq;
    } else {
        int32_t cpus = jlos_fdt_find_node("/cpus");
        if (cpus >= 0) {
            jlos_fdt_get_prop_u32(cpus, "timebase-frequency", &s_timer_freq);
        }
    }
    if (s_timer_freq == 0) {
        printk_err("cntfrq_el0 = 0 and no timebase-frequency, timer disabled\n");
        return;
    }

    timer_write_ctl(0);
    jlos_arch_gic_irq_set_edge(s_timer_intid);
    jlos_irq_handler_init(&s_timer_irq, jlos_active_irq_manager, s_timer_intid);
    s_timer_irq.handle_interrupt = timer_handle_irq;

    s_timer_cs.shift = JLOS_ARCH_TIMER_CS_SHIFT;
    s_timer_cs.mult = (uint32_t)((JLOS_ARCH_TIMER_NS_PER_SEC << JLOS_ARCH_TIMER_CS_SHIFT) / s_timer_freq);
    jlos_clocksource_register(&s_timer_cs);
    jlos_clock_event_register(&s_timer_evt);
    printk_info("freq = %u hz, intid = %u\n", s_timer_freq, s_timer_intid);
}

void jlos_arch_timer_cpu_init(void)
{
    if (!jlos_arch_gic_ready()) {
        return;
    }
    jlos_arch_gic_irq_set_edge(s_timer_intid);
    jlos_arch_gic_irq_unmask(s_timer_intid);
    timer_write_cval(timer_cntpct_read() + s_period_ticks);
    timer_write_ctl(JLOS_ARCH_TIMER_CTL_ENABLE);
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_arch_timer_init);
