/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_FIXMAP_H
#define _JLOS_ARCH_X86_FIXMAP_H

#include <common/types.h>
#include <hal/paging.h>

#define JLOS_FIXADDR_TOP        JLOS_PAGE_ALIGN_DOWN(KERNEL_VIRTUAL_END)
#define JLOS_FIXMAP_SLOT_SIZE   JLOS_PAGE_SIZE

typedef enum {
    JLOS_FIXMAP_APIC_BASE,
    JLOS_FIXMAP_IO_APIC_0,
    JLOS_FIXMAP_IO_APIC_1,
    JLOS_FIXMAP_ACPI_0,
    JLOS_FIXMAP_ACPI_1,
    JLOS_FIXMAP_END
} jlos_fixmap_idx_t;

#define jlos_fixmap_virt(idx) \
    (JLOS_FIXADDR_TOP - ((idx) + 1) * JLOS_FIXMAP_SLOT_SIZE)

void jlos_arch_set_fixmap(jlos_fixmap_idx_t idx, uint32_t phys);
void jlos_arch_set_fixmap_nocache(jlos_fixmap_idx_t idx, uint32_t phys);

void *jlos_fixmap_map(jlos_fixmap_idx_t idx, uint32_t phys);
void *jlos_fixmap_map_nocache(jlos_fixmap_idx_t idx, uint32_t phys);

#endif
