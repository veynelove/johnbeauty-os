#ifndef _JLOS_HAL_PAGING_H
#define _JLOS_HAL_PAGING_H

#include <common/types.h>

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

#endif
