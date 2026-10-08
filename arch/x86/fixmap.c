/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/fixmap.h>
#include <hal/paging.h>

static void jlos_fixmap_set(jlos_fixmap_idx_t idx, uint32_t phys, uint32_t prot)
{
    uint32_t va = jlos_fixmap_virt(idx);
    if (jlos_arch_pte_present(&s_kernel_paging_context, va)) {
        jlos_arch_pte_replace(&s_kernel_paging_context, va, phys, prot);
    } else {
        jlos_arch_map_entry(&s_kernel_paging_context, va, phys, prot);
    }
    jlos_hal_paging_flush_tlb(va);
}

void jlos_arch_set_fixmap(jlos_fixmap_idx_t idx, uint32_t phys)
{
    jlos_fixmap_set(idx, phys, JLOS_PG_KERNEL_RW);
}

void jlos_arch_set_fixmap_nocache(jlos_fixmap_idx_t idx, uint32_t phys)
{
    jlos_fixmap_set(idx, phys, JLOS_PG_KERNEL_RW | JLOS_PG_NOCACHE);
}

void *jlos_fixmap_map(jlos_fixmap_idx_t idx, uint32_t phys)
{
    jlos_arch_set_fixmap(idx, phys);
    return (void *)(jlos_fixmap_virt(idx) + (phys & (JLOS_PAGE_SIZE - 1)));
}

void *jlos_fixmap_map_nocache(jlos_fixmap_idx_t idx, uint32_t phys)
{
    jlos_arch_set_fixmap_nocache(idx, phys);
    return (void *)(jlos_fixmap_virt(idx) + (phys & (JLOS_PAGE_SIZE - 1)));
}
