/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_PAGING_H
#define _JLOS_ARCH_X86_PAGING_H

#include <common/types.h>
#include <hal/paging.h>

#ifdef CONFIG_X86_64
#define JLOS_PAGE_TABLE_ENTRIES                     512
#define JLOS_PAGE_DIR_ENTRIES                       512
#define JLOS_PAGE_PDPT_ENTRIES                      512
#define JLOS_PAGE_PML4_ENTRIES                      512
#define JLOS_PAGING_PT_INDEX_MASK                   0x0001'FF00'0ULL
#define JLOS_PAGING_PD_INDEX_MASK                   0x0000'0003'FE00'0000ULL
#define JLOS_PAGING_PDPT_INDEX_MASK                 0x0000'0007'FC00'0000ULL
#define JLOS_PAGING_PML4_INDEX_MASK                 0xFF80'0000'0000ULL
#define JLOS_PAGING_LARGE_PAGE_SHIFT                21
#else
#define JLOS_PAGE_TABLE_ENTRIES                     1024
#define JLOS_PAGE_DIR_ENTRIES                       1024
#define JLOS_PAGING_PT_INDEX_MASK                   0x000FF000
#define JLOS_PAGING_PD_INDEX_MASK                   0xFFC00000
#define JLOS_PAGING_LARGE_PAGE_SHIFT                22
#endif

#define JLOS_PTE_PRESENT                0x001
#define JLOS_PTE_WRITABLE               0x002
#define JLOS_PTE_USER                   0x004
#define JLOS_PTE_WRITE_THROUGH          0x008
#define JLOS_PTE_CACHE_DISABLE          0x010
#define JLOS_PTE_ACCESSED               0x020
#define JLOS_PTE_DIRTY                  0x040
#define JLOS_PTE_PAT                    0x080
#define JLOS_PTE_GLOBAL                 0x100
#define JLOS_PTE_NX                     0x8000'0000'0000'0000ULL

#define JLOS_PDE_PRESENT                0x001
#define JLOS_PDE_WRITABLE               0x002
#define JLOS_PDE_USER                   0x004
#define JLOS_PDE_WRITE_THROUGH          0x008
#define JLOS_PDE_CACHE_DISABLE          0x010
#define JLOS_PDE_ACCESSED               0x020
#define JLOS_PDE_DIRTY                  0x040
#define JLOS_PDE_PS                    0x080
#define JLOS_PDE_GLOBAL                 0x100

#define JLOS_PTE_KERNEL_RO              (JLOS_PTE_PRESENT)
#define JLOS_PTE_KERNEL_RW              (JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE)
#define JLOS_PTE_USER_RO                (JLOS_PTE_PRESENT | JLOS_PTE_USER)
#define JLOS_PTE_USER_RW                (JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE | JLOS_PTE_USER)
#define JLOS_PTE_USER_COW               (JLOS_PTE_PRESENT | JLOS_PTE_USER)

#define JLOS_PDE_KERNEL_4MB             (JLOS_PDE_PRESENT | JLOS_PDE_PS | JLOS_PDE_WRITABLE)
#define JLOS_PDE_USER_4MB_RW            (JLOS_PDE_PRESENT | JLOS_PDE_PS | JLOS_PDE_WRITABLE | JLOS_PDE_USER)

#ifdef CONFIG_X86_64
#define JLOS_PAGING_ADDR_MASK           0x000F'FFFF'FFFF'F000ULL
#else
#define JLOS_PAGING_ADDR_MASK           0xFFFF'F000
#endif

#define JLOS_PAGING_LARGE_SIZE          (JLOS_PAGE_SIZE * JLOS_PAGE_TABLE_ENTRIES)
#define JLOS_PAGING_LARGE_ADDR_MASK     (~((uintptr_t)JLOS_PAGING_LARGE_SIZE - 1))

#ifdef CONFIG_X86_64
typedef uint64_t jlos_page_table_entry_t;
typedef uint64_t jlos_page_dir_entry_t;
#else
typedef uint32_t jlos_page_table_entry_t;
typedef uint32_t jlos_page_dir_entry_t;
#endif

typedef struct {
    jlos_page_table_entry_t entries[JLOS_PAGE_TABLE_ENTRIES];
} jlos_page_table_t;

typedef struct {
    jlos_page_dir_entry_t entries[JLOS_PAGE_DIR_ENTRIES];
} jlos_page_dir_t;

#ifdef CONFIG_X86_64
typedef struct {
    jlos_page_dir_entry_t entries[JLOS_PAGE_PDPT_ENTRIES];
} jlos_page_pdpt_t;

typedef struct {
    jlos_page_dir_entry_t entries[JLOS_PAGE_PML4_ENTRIES];
} jlos_page_pml4_t;
#endif

#ifdef CONFIG_X86_64
static inline uint32_t jlos_paging_get_pml4_index(uintptr_t virtual_addr)
{
    return (uint32_t)((virtual_addr >> 39) & 0x1FF);
}

static inline uint32_t jlos_paging_get_pdpt_index(uintptr_t virtual_addr)
{
    return (uint32_t)((virtual_addr >> 30) & 0x1FF);
}

static inline uint32_t jlos_paging_get_page_dir_index(uintptr_t virtual_addr)
{
    return (uint32_t)((virtual_addr >> 21) & 0x1FF);
}

static inline uint32_t jlos_paging_get_page_table_index(uintptr_t virtual_addr)
{
    return (uint32_t)((virtual_addr >> 12) & 0x1FF);
}

static inline uintptr_t jlos_paging_get_page_offset(uintptr_t virtual_addr)
{
    return virtual_addr & 0xFFF;
}
#else
static inline uint32_t jlos_paging_get_page_dir_index(uintptr_t virtual_addr)
{
    return (uint32_t)((virtual_addr >> 22) & 0x3FF);
}

static inline uint32_t jlos_paging_get_page_table_index(uintptr_t virtual_addr)
{
    return (uint32_t)((virtual_addr >> 12) & 0x3FF);
}

static inline uintptr_t jlos_paging_get_page_offset(uintptr_t virtual_addr)
{
    return virtual_addr & 0xFFF;
}
#endif

#endif
