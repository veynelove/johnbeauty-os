/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_x86_LAPIC_H
#define _JLOS_ARCH_x86_LAPIC_H

/* Local Advanced Programmable Interrupt Controller */

#include <common/types.h>

#define JLOS_ARCH_IA32_APIC_BASE_MSR    0x1b

#define JLOS_ARCH_MSR_APIC_BASE_ENABLE  0x800

#define JLOS_ARCH_APIC_ID_OFFSET        0x020
#define JLOS_ARCH_APIC_VERSION_OFFSET   0x030
#define JLOS_ARCH_APIC_EOI_OFFSET       0x0b0
#define JLOS_ARCH_APIC_SPURIOUS_OFFSET  0x0f0
#define JLOS_ARCH_APIC_ICR_HIGH_OFFSET  0x310
#define JLOS_ARCH_APIC_ICR_LOW_OFFSET   0x300

#define JLOS_ARCH_APIC_SPURIOUS_ENABLE  0x100
#define JLOS_ARCH_APIC_SPURIOUS_VECTOR  0xFF

#define JLOS_ARCH_APIC_ICR_INIT         0x00004500
#define JLOS_ARCH_APIC_ICR_STARTUP      0x00004600

#define JLOS_ARCH_IPI_TLB_SHOOTDOWN_VECTOR  0xfb
#define JLOS_ARCH_IPI_RESCHEDULE_VECTOR     0xfc
#define JLOS_ARCH_IPI_VECTOR_FIRST          JLOS_ARCH_IPI_TLB_SHOOTDOWN_VECTOR

#define JLOS_ARCH_APIC_LVT_TIMER_OFFSET     0x320
#define JLOS_ARCH_APIC_TIMER_DIV_OFFSET     0x3e0
#define JLOS_ARCH_APIC_TIMER_INITIAL_OFFSET 0x380
#define JLOS_ARCH_APIC_TIMER_CURRENT_OFFSET 0x390

#define JLOS_ARCH_APIC_LVT_TIMER_MASKED     0x10000
#define JLOS_ARCH_APIC_LVT_TIMER_PERIODIC   0x20000

#define JLOS_ARCH_APIC_TIMER_DIV_BY_1       0xb
#define JLOS_ARCH_LAPIC_TIMER_VECTOR        0xef

void jlos_arch_lapic_init(void);
uint32_t jlos_arch_lapic_id(void);
void jlos_arch_lapic_eoi(void);

void jlos_arch_lapic_send_ipi(uint32_t apic_id, uint32_t icr_low);
void jlos_arch_smp_send_ipi(uint32_t cpu, uint32_t vector);

void jlos_arch_lapic_timer_calibrate(void);

#endif
