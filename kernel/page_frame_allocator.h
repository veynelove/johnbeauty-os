#ifndef __JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H
#define __JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H

#include <common/types.h>

#define JLOS_PAGE_FRAME_SIZE            4096
#define JLOS_PAGE_FRAME_REFCOUNT_MAX    255

void jlos_page_frame_allocator_init(uint32_t kernel_end_addr);

void *jlos_page_frame_malloc(void);
void jlos_page_frame_free(void *addr);

void *jlos_page_frame_reserve_bulk(uint32_t num_frames);
void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end);

uint32_t jlos_page_frame_get_total(void);
uint32_t jlos_page_frame_get_free(void);

void jlos_page_frame_refcount_inc(uint32_t phys_addr);
void jlos_page_frame_refcount_dec(uint32_t phys_addr);
uint8_t jlos_page_frame_refcount_get(uint32_t phys_addr);

#endif
