/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H
#define _JLOS_KERNEL_PAGE_FRAME_ALLOCATOR_H

#include <common/types.h>
#include <kernel/paging.h>
#include <hal/atomic.h>
#include <dsa/list.h>
#include <dsa/rbtree.h>

#define JLOS_PAGE_FRAME_SIZE            JLOS_PAGE_SIZE
#define JLOS_PAGE_FRAME_REFCOUNT_MAX    255

#define JLOS_PFA_MAX_ORDER              10
#define JLOS_PFA_BUDDY_ORDER_INVALID    0xFF

#define JLOS_PFA_FLAG_OCCUPIED          0x01

#define JLOS_PFA_FREE_N_MAX             1024

#define JLOS_PFA_PERCPU_HIGN            16
#define JLOS_PFA_PERCPU_BATCH           8

typedef enum {
    JLOS_PAGE_FRAME_TYPE_FREE       = 0,
    JLOS_PAGE_FRAME_TYPE_SLAB_OBJ   = 1,
    JLOS_PAGE_FRAME_TYPE_KV_CONTIG  = 2,
    JLOS_PAGE_FRAME_TYPE_KV_HEAP    = 3,
    JLOS_PAGE_FRAME_TYPE_KERN_STACK = 4,
    JLOS_PAGE_FRAME_TYPE_PAGE_TABLE = 5,
    JLOS_PAGE_FRAME_TYPE_FILE_PAGE  = 6,
} jlos_page_frame_type_t;

typedef struct jlos_page_t {
union {
jlos_list_head_t free_list;
struct {
    void *mapping;
    uint64_t index;
} file;
void *owner;
}                       u;
    jlos_list_head_t    lru;
    jlos_rbtree_node_t  node;
    uint8_t             flags;
    uint8_t             type;
    uint8_t             order;
    uint8_t             pt_present_count;
    uint8_t             page_state;
    uint8_t             _pad[3];
    jlos_atomic_t       refcount;
    void                *bufs;
} jlos_page_t;

typedef struct {
    jlos_list_head_t free_list;
    uint32_t count;
} jlos_pfa_percpu_t;

void jlos_pfa_boot_alloc_init(uint32_t start_phys);
void *jlos_pfa_boot_alloc(uint32_t size);
void *jlos_pfa_boot_alloc_page(void);
uint32_t jlos_pfa_boot_alloc_get_end(void);

void jlos_page_frame_allocator_init(void);

void *jlos_page_frame_malloc(void);
void jlos_page_frame_free(void *addr);

void *jlos_page_frame_alloc_n(uint32_t num_frames);
void jlos_page_frame_free_n(void *addr, uint32_t num_frames);

void *jlos_page_frame_alloc_order(uint32_t order);
void  jlos_page_frame_free_order(void *addr, uint32_t order);

void jlos_page_frame_mark_occupied(uint32_t phys_start, uint32_t phys_end);

uint32_t jlos_page_frame_get_total(void);
uint32_t jlos_page_frame_get_free(void);

void jlos_page_frame_refcount_inc(uint32_t phys_addr);
void jlos_page_frame_refcount_dec(uint32_t phys_addr);
uint8_t jlos_page_frame_refcount_get(uint32_t phys_addr);

jlos_page_frame_type_t jlos_page_frame_get_type(uint32_t phys);
void *jlos_page_frame_get_owner(uint32_t phys);
void jlos_page_frame_set_owner_type(uint32_t phys, void *owner, jlos_page_frame_type_t type);
void jlos_page_frame_clear_owner_type(uint32_t phys);

jlos_page_t *jlos_page_frame_to_page(void *addr);
void *jlos_page_frame_page_addr(jlos_page_t *page);

void jlos_page_frame_print_buddy(void);

bool jlos_page_frame_contains_phys(uint32_t phys);
uint32_t jlos_page_frame_start_phys(void);

void jlos_page_frame_pt_present_count_set(uint32_t phys, uint8_t count);
uint32_t jlos_page_frame_pt_present_count_get(uint32_t phys);
void jlos_page_frame_pt_present_count_inc(uint32_t phys);
void jlos_page_frame_pt_present_count_dec(uint32_t phys);
#endif
