/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/ioapic.h>
#include <arch/x86/fixmap.h>
#include <arch/x86/smp.h>
#include <hal/io.h>

#define JLOS_KERNEL_LOG_SUBSYS "ioapic"
#include <kernel/printk.h>

#define JLOS_MAX_IOAPIC 2

static jlos_ioapic_t    s_ioapics[JLOS_MAX_IOAPIC];
static uint32_t         s_ioapic_count;
static bool             s_ioapic_enabled;
static uint32_t         s_irq_gsi[JLOS_NR_IRQS];
static uint16_t         s_irq_flags[JLOS_NR_IRQS];

static uint32_t jlos_ioapic_read(jlos_ioapic_t *ioapic, uint32_t reg)
{
    jlos_io_mmio_write32((volatile void *)(ioapic->base + JLOS_IOAPIC_IOREGSEL_OFFSET), reg);
    return jlos_io_mmio_read32((volatile void *)(ioapic->base + JLOS_IOAPIC_IOWIN_OFFSET));
}

static void jlos_ioapic_write(jlos_ioapic_t *ioapic, uint32_t reg, uint32_t value)
{
    jlos_io_mmio_write32((volatile void *)(ioapic->base + JLOS_IOAPIC_IOREGSEL_OFFSET), reg);
    jlos_io_mmio_write32((volatile void *)(ioapic->base + JLOS_IOAPIC_IOWIN_OFFSET), value);
}

static uint32_t jlos_ioapic_max_gsi(jlos_ioapic_t *ioapic)
{
    uint32_t ver = jlos_ioapic_read(ioapic, JLOS_IOAPIC_REG_VER);
    return ((ver >> 16) & 0xff) + 1;
}

static jlos_ioapic_t *jlos_ioapic_for_gsi(uint32_t gsi)
{
    for (uint32_t i = 0; i < s_ioapic_count; i++) {
        jlos_ioapic_t *ioapic = &s_ioapics[i];
        if (gsi >= ioapic->gsi_base && gsi < ioapic->gsi_base + ioapic->gsi_count) {
            return ioapic;
        }
    }
    return NULL;
}

static void jlos_ioapic_set_entry(uint32_t gsi, uint32_t vector, uint32_t dest, uint16_t flags)
{
    jlos_ioapic_t *ioapic = jlos_ioapic_for_gsi(gsi);
    if (!ioapic) {
        return;
    }
    uint32_t idx = gsi - ioapic->gsi_base;
    uint32_t reg = JLOS_IOAPIC_REG_REDTBL + 2 * idx;
    uint32_t lo = vector | JLOS_IOAPIC_DEST_PHYSICAL | JLOS_IOAPIC_DELIVERY_FIXED |
        ((uint32_t)(flags & 1) << 13) | ((uint32_t)((flags >> 2) & 1) << 15) | JLOS_IOAPIC_REDTBL_MASK;
    jlos_ioapic_write(ioapic, reg, lo);
    jlos_ioapic_write(ioapic, reg + 1, dest << 24);
}

bool jlos_ioapic_enabled(void)
{
    return s_ioapic_enabled;
}

void jlos_ioapic_init(void)
{
    if (s_ioapic_count == 0) {
        return;
    }
    uint32_t dest = jlos_arch_smp_cpu_apic_id(0);
    jlos_ioapic_mask_all();

    for (uint32_t irq = 0; irq < JLOS_NR_IRQS; irq++) {
        if (s_irq_flags[irq] != 0) {
            uint32_t gsi = s_irq_gsi[irq];
            jlos_ioapic_set_entry(gsi, JLOS_IRQ_VECTOR_BASE + gsi, dest, s_irq_flags[irq]);
        }
    }

    s_ioapic_enabled = true;
    for (uint32_t i = 0; i < s_ioapic_count; i++) {
        printk_info("ioapic id = %u, gsi_base = %u, gsi_count = %u\n",
            s_ioapics[i].id, s_ioapics[i].gsi_base, s_ioapics[i].gsi_count);
    }
}

void jlos_ioapic_mask_all(void)
{
    uint32_t dest = jlos_arch_smp_cpu_apic_id(0);
    for (uint32_t i = 0; i < s_ioapic_count; i++) {
        jlos_ioapic_t *ioapic = &s_ioapics[i];
        for (uint32_t g = 0; g < ioapic->gsi_count; g++) {
            uint32_t gsi = ioapic->gsi_base + g;
            jlos_ioapic_set_entry(gsi, JLOS_IRQ_VECTOR_BASE + gsi, dest, 0);
        }
    }
}

void jlos_ioapic_unmask_irq(uint8_t irq)
{
    uint32_t gsi = jlos_irq_to_gsi(irq);
    jlos_ioapic_t *ioapic = jlos_ioapic_for_gsi(gsi);
    if (!ioapic) {
        return;
    }
    uint32_t idx = gsi - ioapic->gsi_base;
    uint32_t reg = JLOS_IOAPIC_REG_REDTBL + 2 * idx;
    uint32_t lo = jlos_ioapic_read(ioapic, reg);
    lo &= ~JLOS_IOAPIC_REDTBL_MASK;
    jlos_ioapic_write(ioapic, reg, lo);
}

void jlos_ioapic_routes_init(void)
{
    for (uint32_t irq = 0; irq < JLOS_NR_IRQS; irq++) {
        s_irq_gsi[irq] = irq;
        s_irq_flags[irq] = 0;
    }
}

void jlos_ioapic_add_iso(uint8_t source, uint32_t gsi, uint16_t flags)
{
    s_irq_gsi[source] = gsi;
    s_irq_flags[source] = flags;
}

uint32_t jlos_irq_to_gsi(uint8_t irq)
{
    return s_irq_gsi[irq];
}

void jlos_ioapic_add_from_madt(uint8_t ioapic_id, uint32_t address, uint32_t gsi_base)
{
    if (s_ioapic_count >= JLOS_MAX_IOAPIC) {
        return;
    }
    jlos_ioapic_t *ioapic = &s_ioapics[s_ioapic_count];
    ioapic->id = ioapic_id;
    ioapic->gsi_base = gsi_base;
    jlos_arch_set_fixmap_nocache(JLOS_FIXMAP_IO_APIC_0 + s_ioapic_count, address);
    ioapic->base = jlos_fixmap_virt(JLOS_FIXMAP_IO_APIC_0 + s_ioapic_count);
    ioapic->gsi_count = jlos_ioapic_max_gsi(ioapic);
    s_ioapic_count++;
}
