#ifndef __JLOS_KERNEL_PAGING_H
#define __JLOS_KERNEL_PAGING_H

#include <common/types.h>

#define JLOS_PAGE_SIZE                              4096
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

#define KERNEL_VIRTUAL_BASE             0xC0000000

#define VIRT_TO_PHYS(addr)          ((uint32_t)(addr) - KERNEL_VIRTUAL_BASE)
#define PHYS_TO_VIRT(addr)          ((uint32_t)(addr) + KERNEL_VIRTUAL_BASE)

#define JLOS_PAGE_ALIGN_DOWN(addr)  ((addr) & ~(JLOS_PAGE_SIZE - 1))
#define JLOS_PAGE_ALIGN_UP(addr)    JLOS_PAGE_ALIGN_DOWN((addr) + JLOS_PAGE_SIZE - 1)
#define JLOS_PAGE_IS_ALIGNED(addr)  (((addr) & (JLOS_PAGE_SIZE - 1)) == 0)
#define JLOS_PAGE_ADDR_MASK         (~(JLOS_PAGE_SIZE - 1))
#define JLOS_PDE_4MB_ADDR_MASK      (~(JLOS_PAGE_SIZE * JLOS_PAGE_TABLE_ENTRIES - 1))

typedef uint32_t jlos_page_table_entry_t;
typedef uint32_t jlos_page_dir_entry_t;

typedef struct {
    jlos_page_table_entry_t entries[JLOS_PAGE_TABLE_ENTRIES];
} jlos_page_table_t;

typedef struct {
    jlos_page_dir_entry_t entries[JLOS_PAGE_DIR_ENTRIES];
} jlos_page_dir_t;

typedef struct {
    jlos_page_dir_t *page_dir;
    uint32_t num_page_tables;
} jlos_paging_context_t;

extern jlos_paging_context_t *jlos_active_paging_context;

void jlos_paging_context_init(jlos_paging_context_t *self);
void jlos_paging_context_destroy(jlos_paging_context_t *self);

bool jlos_paging_map(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
bool jlos_paging_unmap(jlos_paging_context_t *self, uint32_t virtual_addr);
uint32_t jlos_paging_get_physical_addr(jlos_paging_context_t *self, uint32_t virtual_addr);
bool jlos_paging_map_range(jlos_paging_context_t *self, uint32_t virtual_addr_start,
    uint32_t physical_addr_start, size_t size, uint32_t flags);

void jlos_paging_enable(jlos_paging_context_t *self);
void jlos_paging_disable(void);
void jlos_paging_switch(jlos_paging_context_t *self);
void jlos_paging_change_flags_range(jlos_paging_context_t *self, uint32_t virtual_addr_start, uint32_t virtual_addr_end,
    uint32_t flags);

void jlos_paging_initialize_kernel_paging(void);
bool jlos_paging_is_user_accessible(uint32_t virtual_addr, uint32_t len);

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
