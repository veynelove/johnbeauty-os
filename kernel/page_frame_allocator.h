#ifndef _JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H
#define _JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H

#include <common/types.h>
#include <kernel/paging.h>

#define JLOS_PAGE_FRAME_SIZE            JLOS_PAGE_SIZE
#define JLOS_PAGE_FRAME_REFCOUNT_MAX    255

#define JLOS_PFA_MAX_ORDER              10
#define JLOS_PFA_BUDDY_ORDER_INVALID    0xFF

#define JLOS_PFA_BIT_MAP_FRAME_VALUE(frame) (s_bitmap[(frame) / 8] & (1 << ((frame) % 8)))

#define JLOS_BUDDY_NODE_MAGIC 0xBADDF00DU

typedef struct free_order_node_t {
    struct free_order_node_t    *next;
    struct free_order_node_t    *prev;
    uint32_t                    phys_frame;
    uint32_t                    magic;   /* canary, buddy_insert 写, buddy_remove 校验 */
} free_order_node_t;

void jlos_pfa_boot_alloc_init(uint32_t start_phys);
void *jlos_pfa_boot_alloc(uint32_t size);
void *jlos_pfa_boot_alloc_page(void);
uint32_t jlos_pfa_boot_alloc_get_end(void);

void jlos_page_frame_allocator_init(void);

void *jlos_page_frame_malloc(void);
void jlos_page_frame_free(void *addr);

void jlos_page_frame_free_bulk(uint32_t phys_start, uint32_t num_frames);

void *jlos_page_frame_reserve_bulk(uint32_t num_frames);
void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end);

uint32_t jlos_page_frame_get_total(void);
uint32_t jlos_page_frame_get_free(void);

void jlos_page_frame_refcount_inc(uint32_t phys_addr);
void jlos_page_frame_refcount_dec(uint32_t phys_addr);
uint8_t jlos_page_frame_refcount_get(uint32_t phys_addr);

typedef enum {
    JLOS_PAGE_FRAME_TYPE_FREE       = 0,
    JLOS_PAGE_FRAME_TYPE_SLAB_OBJ   = 1,
    JLOS_PAGE_FRAME_TYPE_KV_CONTIG  = 2,
    JLOS_PAGE_FRAME_TYPE_KV_HEAP    = 3,
    JLOS_PAGE_FRAME_TYPE_KERN_STACK = 4,
} jlos_page_frame_type_t;

jlos_page_frame_type_t jlos_page_frame_get_type(uint32_t phys);
void *jlos_page_frame_get_owner(uint32_t phys);
void jlos_page_frame_set_owner_type(uint32_t phys, void *owner, jlos_page_frame_type_t type);
void jlos_page_frame_clear_owner_type(uint32_t phys);

void jlos_page_frame_print_buddy(void);

bool jlos_page_frame_contains_phys(uint32_t phys);
uint32_t jlos_page_frame_start_phys(void);
#endif
