#ifndef _JLOS_HAL_PAGING_H
#define _JLOS_HAL_PAGING_H

#include <common/types.h>
#include <hal/spinlock.h>

#define JLOS_PG_READ       0x1
#define JLOS_PG_WRITE      0x2
#define JLOS_PG_EXEC       0x4
#define JLOS_PG_USER       0x8
#define JLOS_PG_GLOBAL     0x10

#define JLOS_PG_KERNEL_RO  (JLOS_PG_READ)
#define JLOS_PG_KERNEL_RW  (JLOS_PG_READ | JLOS_PG_WRITE)
#define JLOS_PG_USER_RO    (JLOS_PG_READ | JLOS_PG_USER)
#define JLOS_PG_USER_RW    (JLOS_PG_READ | JLOS_PG_WRITE | JLOS_PG_USER)

#define JLOS_PAGE_SIZE                  4096

#define KERNEL_VIRTUAL_BASE             0xC0000000
#define USER_VIRTUAL_END                0xBFFFFFFF
#define KERNEL_VIRTUAL_END              0xFFFFFFFF
#define KERNEL_SPACE_SIZE               0x40000000
#define KERNEL_PHYSICAL_MAX             KERNEL_SPACE_SIZE
#define KERNEL_DIRECT_MAP_SIZE          0x38000000
#define KERNEL_HEAP_VIRT_BASE           (KERNEL_VIRTUAL_BASE + KERNEL_DIRECT_MAP_SIZE)

#define VIRT_TO_PHYS(addr)          ((uint32_t)(addr) - KERNEL_VIRTUAL_BASE)
#define PHYS_TO_VIRT(addr)          ((uint32_t)(addr) + KERNEL_VIRTUAL_BASE)

#define JLOS_PAGE_ALIGN_DOWN(addr)  ((addr) & ~(JLOS_PAGE_SIZE - 1))
#define JLOS_PAGE_ALIGN_UP(addr)    JLOS_PAGE_ALIGN_DOWN((addr) + JLOS_PAGE_SIZE - 1)
#define JLOS_PAGE_IS_ALIGNED(addr)  (((addr) & (JLOS_PAGE_SIZE - 1)) == 0)
#define JLOS_PAGE_ADDR_MASK         (~(JLOS_PAGE_SIZE - 1))

typedef struct jlos_paging_context {
    void            *root;
    uint32_t        num_page_tables;
    jlos_spinlock_t lock;
} jlos_paging_context_t;

extern jlos_paging_context_t s_kernel_paging_context;

typedef void *(*jlos_paging_table_alloc_fn)(void);

void jlos_hal_paging_enable(uint32_t page_dir_physical_addr);
void jlos_hal_paging_disable(void);
void jlos_hal_paging_switch(uint32_t page_dir_physical_addr);
void jlos_hal_paging_flush_tlb(uint32_t virtual_addr);
void jlos_hal_paging_flush_all_tlb(void);
uint32_t jlos_hal_paging_get_fault_addr(void);
bool jlos_hal_paging_supports_4mb_pages(void);

struct jlos_paging_context;
struct jlos_paging_context *jlos_hal_paging_get_active_context(void);
void jlos_hal_paging_set_active_context(struct jlos_paging_context *ctx);

void jlos_hal_paging_enable_global_pages(void);
uint32_t jlos_hal_paging_asid_alloc(void);
void jlos_hal_paging_asid_free(uint32_t asid);

bool jlos_arch_pte_present(jlos_paging_context_t *ctx, uint32_t va);
uint32_t jlos_arch_pte_phys(jlos_paging_context_t *ctx, uint32_t va);
void jlos_arch_pte_set(jlos_paging_context_t *ctx, uint32_t va, uint32_t pa, uint32_t prot);
void jlos_arch_pte_set_rw(jlos_paging_context_t *ctx, uint32_t va);
void jlos_arch_pte_make_ro(jlos_paging_context_t *ctx, uint32_t va);
bool jlos_arch_pte_replace(jlos_paging_context_t *ctx, uint32_t va, uint32_t pa, uint32_t prot);

bool jlos_arch_map_entry(jlos_paging_context_t *ctx, uint32_t va, uint32_t pa, uint32_t prot);
bool jlos_arch_map_range_entry(jlos_paging_context_t *ctx, uint32_t va_start, uint32_t pa_start,
    size_t size, uint32_t prot);
bool jlos_arch_unmap_entry(jlos_paging_context_t *ctx, uint32_t va);
uint32_t jlos_arch_get_phys_entry(jlos_paging_context_t *ctx, uint32_t va);
void jlos_arch_change_flags_entry(jlos_paging_context_t *ctx, uint32_t va, uint32_t prot);
bool jlos_arch_need_flush_tlb(jlos_paging_context_t *ctx, uint32_t va);

void jlos_arch_paging_context_tables_destroy(jlos_paging_context_t *ctx);
void jlos_arch_paging_context_tables_clone(jlos_paging_context_t *dst, jlos_paging_context_t *src);
void jlos_arch_paging_initialize_kernel(jlos_paging_table_alloc_fn alloc_fn);
void jlos_arch_paging_print_states(jlos_paging_context_t *ctx);
void jlos_arch_paging_free_user_pages(jlos_paging_context_t *ctx);

#endif
