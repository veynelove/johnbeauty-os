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

#define JLOS_ARCH_APIC_SPURIOUS_ENABLE  0x100
#define JLOS_ARCH_APIC_SPURIOUS_VECTOR  0xFF

void jlos_arch_lapic_init(void);
uint32_t jlos_arch_lapic_id(void);
void jlos_arch_lapic_eoi(void);

#endif
