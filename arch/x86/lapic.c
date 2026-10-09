/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/lapic.h>
#include <arch/x86/fixmap.h>
#include <arch/x86/msr.h>
#include <arch/x86/smp.h>
#include <hal/io.h>
#include <hal/paging.h>
#include <hal/clock_event.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "lapic"
#include <kernel/printk.h>

#define JLOS_PIT_CH2_GATE_PORT  0x61
#define JLOS_PIT_CH2_PORT       0x42
#define JLOS_PIT_CMD_PORT       0x43
#define JLOS_PIT_CALIB_DIVISOR  11932

static uint32_t s_lapic_base;
static uint32_t s_lapic_timer_freq;

static uint32_t jlos_arch_lapic_mmio_read(uint32_t offset)
{
    return jlos_io_mmio_read32((volatile void *)(s_lapic_base + offset));
}

static void jlos_arch_lapic_mmio_write(uint32_t offset, uint32_t value)
{
    jlos_io_mmio_write32((volatile void *)(s_lapic_base + offset), value);
}

void jlos_arch_lapic_init(void)
{
    uint32_t msr_lo, msr_hi;
    jlos_arch_msr_read(JLOS_ARCH_IA32_APIC_BASE_MSR, &msr_lo, &msr_hi);
    if ((msr_lo & JLOS_ARCH_MSR_APIC_BASE_ENABLE) == 0) {
        msr_lo |= JLOS_ARCH_MSR_APIC_BASE_ENABLE;
        jlos_arch_msr_write(JLOS_ARCH_IA32_APIC_BASE_MSR, msr_lo, msr_hi);
    }

    uint32_t phys_base = msr_lo & JLOS_PAGE_ADDR_MASK;
    jlos_arch_set_fixmap_nocache(JLOS_FIXMAP_APIC_BASE, phys_base);
    s_lapic_base = jlos_fixmap_virt(JLOS_FIXMAP_APIC_BASE);

    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_SPURIOUS_OFFSET, JLOS_ARCH_APIC_SPURIOUS_VECTOR | JLOS_ARCH_APIC_SPURIOUS_ENABLE);

    this_cpu_write(jlos_cpu_lapic_id, jlos_arch_lapic_id());
    printk_info("lapic id = %u, base = 0x%x\n", jlos_arch_lapic_id(), phys_base);
}

uint32_t jlos_arch_lapic_id(void)
{
    return jlos_arch_lapic_mmio_read(JLOS_ARCH_APIC_ID_OFFSET) >> 24;
}

void jlos_arch_lapic_eoi(void)
{
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_EOI_OFFSET, 0);
}

void jlos_arch_lapic_send_ipi(uint32_t apic_id, uint32_t icr_low)
{
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_ICR_HIGH_OFFSET, apic_id << 24);
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_ICR_LOW_OFFSET, icr_low);
}

void jlos_arch_smp_send_ipi(uint32_t cpu, uint32_t vector)
{
    jlos_arch_lapic_send_ipi(jlos_arch_smp_cpu_apic_id(cpu), vector);
}

void jlos_arch_lapic_timer_calibrate(void)
{
    jlos_io8_slow_t gate_port, ch2_port, cmd_port;
    jlos_io8_slow_init(&gate_port, JLOS_PIT_CH2_GATE_PORT);
    jlos_io8_slow_init(&ch2_port, JLOS_PIT_CH2_PORT);
    jlos_io8_slow_init(&cmd_port, JLOS_PIT_CMD_PORT);
    uint8_t gate_orig = jlos_io8_slow_read(&gate_port);

    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_LVT_TIMER_OFFSET, JLOS_ARCH_APIC_LVT_TIMER_MASKED);
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_TIMER_DIV_OFFSET, JLOS_ARCH_APIC_TIMER_DIV_BY_1);
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_TIMER_INITIAL_OFFSET, 0xFFFFFFFF);

    jlos_io8_slow_write(&gate_port, gate_orig & ~0x03);
    jlos_io8_slow_write(&cmd_port, 0xb0);
    jlos_io8_slow_write(&ch2_port, (uint8_t)(JLOS_PIT_CALIB_DIVISOR & 0xFF));
    jlos_io8_slow_write(&ch2_port, (uint8_t)((JLOS_PIT_CALIB_DIVISOR >> 8) & 0xFF));

    uint32_t start = jlos_arch_lapic_mmio_read(JLOS_ARCH_APIC_TIMER_CURRENT_OFFSET);
    jlos_io8_slow_write(&gate_port, (gate_orig & ~0x02) | 0x01);
    while ((jlos_io8_slow_read(&gate_port) & 0x20) == 0) {
    }
    uint32_t end = jlos_arch_lapic_mmio_read(JLOS_ARCH_APIC_TIMER_CURRENT_OFFSET);
    jlos_io8_slow_write(&gate_port, gate_orig);

    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_LVT_TIMER_OFFSET, JLOS_ARCH_APIC_LVT_TIMER_MASKED);

    s_lapic_timer_freq = (start - end) * 100;
    printk_info("lapic timer freq = %u hz\n", s_lapic_timer_freq);
}

static void lapic_timer_set_state_oneshot(void)
{
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_LVT_TIMER_OFFSET, JLOS_ARCH_LAPIC_TIMER_VECTOR);
}

static void lapic_timer_set_state_periodic(uint32_t hz)
{
    uint32_t count = s_lapic_timer_freq / hz;
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_TIMER_INITIAL_OFFSET, count);
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_LVT_TIMER_OFFSET, JLOS_ARCH_LAPIC_TIMER_VECTOR | JLOS_ARCH_APIC_LVT_TIMER_PERIODIC);
}

static void lapic_timer_set_state_shutdown(void)
{
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_LVT_TIMER_OFFSET, JLOS_ARCH_APIC_LVT_TIMER_MASKED);
}

static void lapic_timer_set_next_event(uint64_t delta_ns)
{
    uint32_t count = (uint32_t)((s_lapic_timer_freq * delta_ns) / 1000000000ULL);
    if (count == 0) {
        count = 1;
    }
    jlos_arch_lapic_mmio_write(JLOS_ARCH_APIC_TIMER_INITIAL_OFFSET, count);
}

static jlos_clock_event_device_t s_lapic_timer_evt = {
    .name       = "lapic_timer",
    .rating     = 400,
    .features   = JLOS_CLOCK_EVT_FEAT_PERIODIC | JLOS_CLOCK_EVT_FEAT_ONESHOT,
    .set_state_periodic = lapic_timer_set_state_periodic,
    .set_state_oneshot  = lapic_timer_set_state_oneshot,
    .set_state_shutdown = lapic_timer_set_state_shutdown,
    .set_next_event     = lapic_timer_set_next_event,
};

static void lapic_timer_setup(void)
{
    jlos_arch_lapic_timer_calibrate();
    jlos_clock_event_register(&s_lapic_timer_evt);
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, lapic_timer_setup);
JLOS_INITCALL(JLOS_INITCALL_CORE, jlos_arch_lapic_init);
