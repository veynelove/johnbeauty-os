#ifndef __JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H
#define __JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H

#include <common/types.h>

#define JLOS_PAGE_FRAME_SIZE            4096
#define JLOS_PAGE_FRAME_REFCOUNT_MAX    255

#define JLOS_PFA_MAX_ORDER              10
#define JLOS_PFA_BUDDY_ORDER_INVALID    0xFF

#define JLOS_PFA_BIT_MAP_FRAME_VALUE(frame) (s_bitmap[(frame) / 8] & (1 << ((frame) % 8)))

typedef struct free_order_node_t {
    struct free_order_node_t *next;
    uint32_t phys_frame;
} free_order_node_t;

void jlos_pfa_boot_alloc_init(uint32_t start_phys);
void *jlos_pfa_boot_alloc(uint32_t size);
void *jlos_pfa_boot_alloc_page(void);
uint32_t jlos_pfa_boot_alloc_get_end(void);

void jlos_page_frame_allocator_init(void);

void *jlos_page_frame_malloc(void);
void jlos_page_frame_free(void *addr);

void *jlos_page_frame_reserve_bulk(uint32_t num_frames);
void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end);

uint32_t jlos_page_frame_get_total(void);
uint32_t jlos_page_frame_get_free(void);

void jlos_page_frame_refcount_inc(uint32_t phys_addr);
void jlos_page_frame_refcount_dec(uint32_t phys_addr);
uint8_t jlos_page_frame_refcount_get(uint32_t phys_addr);

void jlos_page_frame_print_buddy(void);


#endif
