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
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "lapic"
#include <kernel/printk.h>

static uint32_t s_lapic_base;

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

JLOS_INITCALL(JLOS_INITCALL_CORE, jlos_arch_lapic_init);
