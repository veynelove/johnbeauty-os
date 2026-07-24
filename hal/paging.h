#ifndef __JLOS_HAL_PAGING_H
#define __JLOS_HAL_PAGING_H

#include <common/types.h>

void jlos_hal_paging_enable(uint32_t page_dir_physical_addr);
void jlos_hal_paging_disable(void);
void jlos_hal_paging_switch(uint32_t page_dir_physical_addr);
void jlos_hal_paging_flush_tlb(uint32_t virtual_addr);
uint32_t jlos_hal_paging_get_fault_addr(void);

#endif
