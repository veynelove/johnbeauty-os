/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_IOAPIC_H
#define _JLOS_ARCH_X86_IOAPIC_H

#include <common/types.h>

/* I/O Advanced Programmable Interrupt Controller */

typedef struct {
    uint8_t id;
    uintptr_t base;
    uint32_t gsi_base;
    uint32_t gsi_count;
} jlos_ioapic_t;

#define JLOS_IOAPIC_IOREGSEL_OFFSET 0x00
#define JLOS_IOAPIC_IOWIN_OFFSET    0x10

#define JLOS_IOAPIC_REG_ID          0x00
#define JLOS_IOAPIC_REG_VER         0x01
#define JLOS_IOAPIC_REG_REDTBL      0x10

#define JLOS_IOAPIC_REDTBL_MASK     0x10000

#define JLOS_IOAPIC_DEST_PHYSICAL   0
#define JLOS_IOAPIC_DELIVERY_FIXED  0

#define JLOS_IRQ_VECTOR_BASE        0x20

#define JLOS_NR_IRQS                256

bool jlos_ioapic_enabled(void);
void jlos_ioapic_init(void);
void jlos_ioapic_mask_all(void);
void jlos_ioapic_unmask_irq(uint8_t irq);

void jlos_ioapic_add_from_madt(uint8_t ioapic_id, uint32_t address, uint32_t gsi_base);

void jlos_ioapic_routes_init(void);
void jlos_ioapic_add_iso(uint8_t source, uint32_t gsi, uint16_t flags);
uint32_t jlos_irq_to_gsi(uint8_t irq);

#endif
