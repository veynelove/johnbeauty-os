#ifndef _JLOS_KERNEL_VMA_H
#define _JLOS_KERNEL_VMA_H

#include <common/types.h>
#include <dsa/rbtree.h>
#include <hal/spinlock.h>
#include <hal/atomic.h>

typedef struct jlos_paging_context jlos_paging_context_t;

#define JLOS_VMA_READ   0x01
#define JLOS_VMA_WRITE  0x02
#define JLOS_VMA_EXEC   0x04
#define JLOS_VMA_USER   0x08
#define JLOS_VMA_COW    0x10

typedef enum {
    JLOS_VMA_TYPE_ANON  = 0,
    JLOS_VMA_TYPE_BRK   = 1,
    JLOS_VMA_TYPE_STACK = 2,
    JLOS_VMA_TYPE_FILE  = 3
} jlos_vma_type_t;

typedef struct jlos_vma {
    uint32_t            start;
    uint32_t            end;
    uint32_t            flags;
    jlos_vma_type_t     type;
    jlos_rbtree_node_t  rb_node;
    void                *file;
    uint32_t            offset;
} jlos_vma_t;

typedef struct jlos_mm {
    jlos_paging_context_t   *pc;
    jlos_rbtree_t           vma_tree;
    uint32_t                brk_start;
    uint32_t                brk_end;
    uint32_t                brk_limit;
    jlos_spinlock_t         lock;
    jlos_atomic_t           refcount;
} jlos_mm_t;

jlos_mm_t *jlos_mm_create(void);
void jlos_mm_destroy(jlos_mm_t *mm);
void jlos_mm_ref_inc(jlos_mm_t *mm);
bool jlos_mm_clone_user(jlos_mm_t *dst, jlos_mm_t *src);

jlos_vma_t *jlos_vma_find(jlos_mm_t *mm, uint32_t addr);
jlos_vma_t *jlos_vma_add(jlos_mm_t *mm, uint32_t start, uint32_t end, uint32_t flags, jlos_vma_type_t type);
bool jlos_vma_remove_range(jlos_mm_t *mm, uint32_t start, uint32_t end);
jlos_vma_t *jlos_vma_grow_tail(jlos_mm_t *mm, uint32_t start, uint32_t new_end, uint32_t flags, jlos_vma_type_t type);
bool jlos_vma_shrink_tail(jlos_mm_t *mm, uint32_t new_end, jlos_vma_type_t type);

bool jlos_vma_demand_map(jlos_mm_t *mm, uint32_t fault_addr);

#endif
