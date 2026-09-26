#ifndef _JLOS_KERNEL_PAGING_H
#define _JLOS_KERNEL_PAGING_H

#include <common/types.h>
#include <hal/irq.h>
#include <hal/spinlock.h>
#include <hal/paging.h>

#define JLOS_PAGE_FRAME_FLUSH_ALL_TLB_THRESHOLD 32

void jlos_paging_context_init(jlos_paging_context_t *self);
void jlos_paging_context_destroy(jlos_paging_context_t *self);
void jlos_paging_context_clone(jlos_paging_context_t *dst, jlos_paging_context_t *src);

bool jlos_paging_map(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t physical_addr, uint32_t prot);
bool jlos_paging_map_range(jlos_paging_context_t *self, uint32_t virtual_addr_start,
    uint32_t physical_addr_start, size_t size, uint32_t prot);
bool jlos_paging_unmap(jlos_paging_context_t *self, uint32_t virtual_addr);
uint32_t jlos_paging_get_physical_addr(jlos_paging_context_t *self, uint32_t virtual_addr);

void jlos_paging_change_flags(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t prot);
void jlos_paging_change_flags_range(jlos_paging_context_t *self, uint32_t virtual_addr_start,
    uint32_t virtual_addr_end, uint32_t prot);
bool jlos_paging_cow_range(jlos_paging_context_t *src, jlos_paging_context_t *dst, uint32_t start, uint32_t end);

void jlos_paging_enable(jlos_paging_context_t *self);
void jlos_paging_switch(jlos_paging_context_t *self);
void jlos_paging_initialize_kernel_paging(jlos_paging_table_alloc_fn alloc_fn);
bool jlos_paging_is_user_accessible(jlos_paging_context_t *ctx, uint32_t virtual_addr, uint32_t len);

void jlos_paging_page_fault_handler(jlos_irq_context_t *context);

#endif
