/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_PAGING_H
#define _JLOS_ARCH_ARM64_PAGING_H

#include <common/types.h>
#include <hal/paging.h>

#define JLOS_ARM64_VA_BITS              48UL
#define JLOS_ARM64_ASID_SHIFT           56
#define JLOS_ARM64_MAX_ASID             256
#define JLOS_ARM64_LEVEL_BITS           9UL
#define JLOS_ARM64_PAGE_LEVELS          4UL
#define JLOS_ARM64_PTE_LEVEL            (JLOS_ARM64_PAGE_LEVELS - 1)
#define JLOS_ARM64_TABLE_ENTRIES        (1UL << JLOS_ARM64_LEVEL_BITS)
#define JLOS_ARM64_ENTRY_INDEX_MASK     (JLOS_ARM64_TABLE_ENTRIES - 1)
#define JLOS_ARM64_DESC_SIZE            8UL
#define JLOS_ARM64_TABLE_SIZE           (JLOS_ARM64_TABLE_ENTRIES * JLOS_ARM64_DESC_SIZE)

#define JLOS_ARM64_DESC_VALID           0x1UL
#define JLOS_ARM64_DESC_TABLE           0x3UL
#define JLOS_ARM64_DESC_BLOCK           0x1UL

#define JLOS_ARM64_PTE_ATTRINDX_SHIFT   2
#define JLOS_ARM64_PTE_ATTRINDX_MASK    (0x7UL << JLOS_ARM64_PTE_ATTRINDX_SHIFT)
#define JLOS_ARM64_PTE_AP1_USER         (1UL << 6)
#define JLOS_ARM64_PTE_AP2_RO           (1UL << 7)
#define JLOS_ARM64_PTE_SH_IS            (3UL << 8) // inner shareable
#define JLOS_ARM64_PTE_AF               (1UL << 10)
#define JLOS_ARM64_PTE_NG               (1UL << 11) // non-global
#define JLOS_ARM64_PTE_PXN              (1UL << 53)
#define JLOS_ARM64_PTE_UXN              (1UL << 54)

#define JLOS_ARM64_ATTR_DEVICE          0
#define JLOS_ARM64_ATTR_NORMAL_WB       1
#define JLOS_ARM64_ATTR_NORMAL_NC       2
#define JLOS_ARM64_MAIR_SET             0x0044FF04UL

#define JLOS_ARM64_PTE_ADDR_MASK        0x0000'FFFF'FFFF'F000UL
#define JLOS_ARM64_PMD_ADDR_MASK        0x0000'FFFF'FFE0'0000UL // L2 2mb
#define JLOS_ARM64_PMD_BLOCK_SIZE       0x200000UL
#define JLOS_ARM64_PUD_ADDR_MASK        0x0000'FFFF'C000'0000UL // L1 1gb
#define JLOS_ARM64_PUD_BLOCK_SIZE       0x40000000UL

#define JLOS_ARM64_L0_T1_OFF            (1 * JLOS_PAGE_SIZE)

static inline uint32_t jlos_arm64_level_index(uintptr_t va, uint32_t level)
{
    return (uint32_t)(va >> (JLOS_ARM64_VA_BITS - JLOS_ARM64_LEVEL_BITS * (level + 1))) & (uintptr_t)JLOS_ARM64_ENTRY_INDEX_MASK;
}

#endif
