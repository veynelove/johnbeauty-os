#ifndef _JLOS_ARCH_X86_PGTABLE_H
#define _JLOS_ARCH_X86_PGTABLE_H

#include <common/types.h>
#include <hal/paging.h>

#define JLOS_PAGE_TABLE_ENTRIES                     1024
#define JLOS_PAGE_DIR_ENTRIES                       1024
#define JLOS_PAGING_VIRTUAL_ADDR_MASK               0xFFFFF000
#define JLOS_PAGING_PT_INDEX_MASK                   0x000FF000
#define JLOS_PAGING_PD_INDEX_MASK                   0xFFC00000

#define JLOS_PTE_PRESENT                0x001
#define JLOS_PTE_WRITABLE               0x002
#define JLOS_PTE_USER                   0x004
#define JLOS_PTE_WRITE_THROUGH          0x008
#define JLOS_PTE_CACHE_DISABLE          0x010
#define JLOS_PTE_ACCESSED               0x020
#define JLOS_PTE_DIRTY                  0x040
#define JLOS_PTE_PAT                    0x080
#define JLOS_PTE_GLOBAL                 0x100

#define JLOS_PDE_PRESENT                0x001
#define JLOS_PDE_WRITABLE               0x002
#define JLOS_PDE_USER                   0x004
#define JLOS_PDE_WRITE_THROUGH          0x008
#define JLOS_PDE_CACHE_DISABLE          0x010
#define JLOS_PDE_ACCESSED               0x020
#define JLOS_PDE_DIRTY                  0x040
#define JLOS_PDE_4MB                    0x080
#define JLOS_PDE_GLOBAL                 0x100

#define JLOS_PTE_KERNEL_RO              (JLOS_PTE_PRESENT)
#define JLOS_PTE_KERNEL_RW              (JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE)
#define JLOS_PTE_USER_RO                (JLOS_PTE_PRESENT | JLOS_PTE_USER)
#define JLOS_PTE_USER_RW                (JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE | JLOS_PTE_USER)
#define JLOS_PTE_USER_COW               (JLOS_PTE_PRESENT | JLOS_PTE_USER)

#define JLOS_PDE_KERNEL_4MB             (JLOS_PDE_PRESENT | JLOS_PDE_4MB | JLOS_PDE_WRITABLE)
#define JLOS_PDE_USER_4MB_RW            (JLOS_PDE_PRESENT | JLOS_PDE_4MB | JLOS_PDE_WRITABLE | JLOS_PDE_USER)

#define JLOS_PDE_4MB_ADDR_MASK      (~(JLOS_PAGE_SIZE * JLOS_PAGE_TABLE_ENTRIES - 1))
#define JLOS_PDE_4MB_SIZE           (JLOS_PAGE_SIZE * JLOS_PAGE_TABLE_ENTRIES)

typedef uint32_t jlos_page_table_entry_t;
typedef uint32_t jlos_page_dir_entry_t;

typedef struct {
    jlos_page_table_entry_t entries[JLOS_PAGE_TABLE_ENTRIES];
} jlos_page_table_t;

typedef struct {
    jlos_page_dir_entry_t entries[JLOS_PAGE_DIR_ENTRIES];
} jlos_page_dir_t;

static inline uint32_t jlos_paging_get_page_dir_index(uint32_t virtual_addr)
{
    return (virtual_addr >> 22) & 0x3FF;
}

static inline uint32_t jlos_paging_get_page_table_index(uint32_t virtual_addr)
{
    return (virtual_addr >> 12) & 0x3FF;
}

static inline uint32_t jlos_paging_get_page_offset(uint32_t virtual_addr)
{
    return virtual_addr & 0xFFF;
}

#endif
