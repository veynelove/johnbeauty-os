#ifndef __JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H
#define __JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H

#include <common/types.h>

#define JLOS_PAGE_FRAME_SIZE 4096

void jlos_page_frame_allocator_init(uint32_t start_addr, uint32_t end_addr, uint32_t kernel_end_addr);
void *jlos_page_frame_malloc(void);
void jlos_page_frame_free(void *addr);
uint32_t jlos_page_frame_get_total(void);
uint32_t jlos_page_frame_get_free(void);

#endif
