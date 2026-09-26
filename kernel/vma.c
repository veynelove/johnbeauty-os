#include <kernel/vma.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "vma"
#include <kernel/printk.h>

static jlos_memory_slab_cache_t *s_vma_cache;
static jlos_memory_slab_cache_t *s_mm_cache;
static jlos_memory_slab_cache_t *s_pctx_cache;

static int jlos_vma_compare(const jlos_rbtree_node_t *a, const jlos_rbtree_node_t *b)
{
    const jlos_vma_t *va = jlos_rbtree_entry(a, jlos_vma_t, rb_node);
    const jlos_vma_t *vb = jlos_rbtree_entry(b, jlos_vma_t, rb_node);
    if (va->start < vb->start) {
        return -1;
    }
    if (va->start > vb->start) {
        return 1;
    }
    return 0;
}

static int jlos_vma_cmp_addr(const jlos_rbtree_node_t *node, const void *key)
{
    const jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
    uint32_t addr = *(const uint32_t *)key;
    if (vma->start < addr) {
        return -1;
    }
    if (vma->start > addr) {
        return 1;
    }
    return 0;
}

jlos_mm_t *jlos_mm_create(void)
{
    jlos_mm_t *mm = (jlos_mm_t *)jlos_memory_slab_cache_alloc(s_mm_cache);
    if (!mm) {
        return NULL;
    }
    mm->pc = (jlos_paging_context_t *)jlos_memory_slab_cache_alloc(s_pctx_cache);
    if (!mm->pc) {
        jlos_kfree(mm);
        return NULL;
    }
    jlos_paging_context_init(mm->pc);
    jlos_rbtree_init(&mm->vma_tree);
    jlos_spinlock_init(&mm->lock);
    mm->brk_start = 0;
    mm->brk_end = 0;
    mm->brk_limit = 0;
    jlos_atomic_set(&mm->refcount, 1);
    return mm;
}

void jlos_mm_destroy(jlos_mm_t *mm)
{
    if (!mm) {
        return;
    }
    if (jlos_atomic_dec_return(&mm->refcount) != 0) {
        return;
    }
    jlos_rbtree_node_t *node = jlos_rbtree_first(&mm->vma_tree);
    while (node) {
        jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
        jlos_rbtree_remove(&mm->vma_tree, node);
        jlos_kfree(vma);
        node = jlos_rbtree_first(&mm->vma_tree);
    }
    if (mm->pc) {
        jlos_paging_context_destroy(mm->pc);
        jlos_kfree(mm->pc);
    }
    jlos_kfree(mm);
}

void jlos_mm_ref_inc(jlos_mm_t *mm)
{
    if (mm) {
        jlos_atomic_inc(&mm->refcount);
    }
}

bool jlos_mm_clone_user(jlos_mm_t *dst, jlos_mm_t *src)
{
    if (!dst || !src || !dst->pc || !src->pc) {
        return false;
    }
    dst->brk_start = src->brk_start;
    dst->brk_end = src->brk_end;
    dst->brk_limit = src->brk_limit;
    jlos_rbtree_node_t *node = jlos_rbtree_first(&src->vma_tree);
    while (node) {
        jlos_vma_t *svma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
        jlos_vma_t *dvma = (jlos_vma_t *)jlos_memory_slab_cache_alloc(s_vma_cache);
        if (!dvma) {
            return false;
        }
        *dvma = *svma;
        dvma->flags |= JLOS_VMA_COW;
        jlos_rbtree_insert(&dst->vma_tree, &dvma->rb_node, jlos_vma_compare);
        jlos_paging_cow_range(src->pc, dst->pc, svma->start, svma->end);
        node = jlos_rbtree_next(&src->vma_tree, node);
    }
    return true;
}

jlos_vma_t *jlos_vma_find(jlos_mm_t *mm, uint32_t addr)
{
    if (!mm) {
        return NULL;
    }
    jlos_rbtree_node_t *node = jlos_rbtree_find_key_le(&mm->vma_tree, &addr, jlos_vma_cmp_addr);
    if (!node) {
        return NULL;
    }
    jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
    if (addr < vma->end) {
        return vma;
    }
    return NULL;
}

jlos_vma_t *jlos_vma_add(jlos_mm_t *mm, uint32_t start, uint32_t end, uint32_t flags, jlos_vma_type_t type)
{
    if (!mm || end <= start) {
        return NULL;
    }
    jlos_vma_t *vma = (jlos_vma_t *)jlos_memory_slab_cache_alloc(s_vma_cache);
    if (!vma) {
        return NULL;
    }
    vma->start = JLOS_PAGE_ALIGN_DOWN(start);
    vma->end = JLOS_PAGE_ALIGN_UP(end);
    vma->flags = flags;
    vma->type = type;
    vma->file = NULL;
    vma->offset = 0;
    jlos_rbtree_insert(&mm->vma_tree, &vma->rb_node, jlos_vma_compare);
    return vma;
}

bool jlos_vma_remove_range(jlos_mm_t *mm, uint32_t start, uint32_t end)
{
    if (!mm || !mm->pc || end <= start) {
        return false;
    }
    start = JLOS_PAGE_ALIGN_DOWN(start);
    end = JLOS_PAGE_ALIGN_UP(end);
    jlos_rbtree_node_t *node = jlos_rbtree_first(&mm->vma_tree);
    while (node) {
        jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
        jlos_rbtree_node_t *next = jlos_rbtree_next(&mm->vma_tree, node);
        if (vma->start >= end) {
            break;
        }
        if (vma->end > start) {
            uint32_t ov_s = (vma->start > start) ? vma->start : start;
            uint32_t ov_e = (vma->end < end) ? vma->end : end;
            for (uint32_t i = ov_s; i < ov_e; i += JLOS_PAGE_FRAME_SIZE) {
                jlos_paging_unmap(mm->pc, i);
            }
            if (ov_s <= vma->start && ov_e >= vma->end) {
                jlos_rbtree_remove(&mm->vma_tree, node);
                jlos_kfree(vma);
            } else if (ov_s <= vma->start) {
                vma->start = ov_e;
            } else if (ov_e >= vma->end) {
                vma->end = ov_s;
            } else {
                jlos_vma_t *tail = (jlos_vma_t *)jlos_memory_slab_cache_alloc(s_vma_cache);
                if (!tail) {
                    node = next;
                    continue;
                }
                *tail = *vma;
                tail->start = ov_e;
                tail->end = ov_s;
                jlos_rbtree_insert(&mm->vma_tree, &tail->rb_node, jlos_vma_compare);
            }
        }
        node = next;
    }
    return true;
}

jlos_vma_t *jlos_vma_grow_tail(jlos_mm_t *mm, uint32_t start, uint32_t new_end, uint32_t flags, jlos_vma_type_t type)
{
    if (!mm || new_end <= start) {
        return NULL;
    }
    start = JLOS_PAGE_ALIGN_DOWN(start);
    new_end = JLOS_PAGE_ALIGN_UP(new_end);
    jlos_rbtree_node_t *node = jlos_rbtree_find_key(&mm->vma_tree, &start, jlos_vma_cmp_addr);
    if (node) {
        jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
        if (vma->type == type) {
            vma->end = new_end;
            return vma;
        }
    }
    return jlos_vma_add(mm, start, new_end, flags, type);
}

bool jlos_vma_shrink_tail(jlos_mm_t *mm, uint32_t new_end, jlos_vma_type_t type)
{
    if (!mm || !mm->pc) {
        return false;
    }
    new_end = JLOS_PAGE_ALIGN_UP(new_end);
    jlos_rbtree_node_t *node = jlos_rbtree_first(&mm->vma_tree);
    while (node) {
        jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
        if (vma->type == type) {
            if (new_end <= vma->start) {
                for (uint32_t i = vma->start; i < vma->end; i += JLOS_PAGE_FRAME_SIZE) {
                    jlos_paging_unmap(mm->pc, i);
                }
                jlos_rbtree_remove(&mm->vma_tree, node);
                jlos_kfree(vma);
            } else if (new_end < vma->end) {
                for (uint32_t i = new_end; i < vma->end; i += JLOS_PAGE_FRAME_SIZE) {
                    jlos_paging_unmap(mm->pc, i);
                }
                vma->end = new_end;
            }
            return true;
        }
        node = jlos_rbtree_next(&mm->vma_tree, node);
    }
    return false;
}

bool jlos_vma_demand_map(jlos_mm_t *mm, uint32_t fault_addr)
{
    if (!mm || !mm->pc) {
        return false;
    }
    jlos_vma_t *vma = jlos_vma_find(mm, fault_addr);
    if (!vma) {
        printk_err("no vma addr=0x%x\n", fault_addr);
        return false;
    }
    uint32_t page = JLOS_PAGE_ALIGN_DOWN(fault_addr);
    uint32_t flags = (vma->flags & JLOS_VMA_WRITE) ? JLOS_PTE_USER_RW : JLOS_PTE_USER_RO;
    void *frame = jlos_page_frame_malloc();
    if (!frame) {
        printk_err("oom addr=0x%x\n", fault_addr);
        return false;
    }
    jlos_memset(frame, 0, JLOS_PAGE_FRAME_SIZE);
    if (!jlos_paging_map(mm->pc, page, VIRT_TO_PHYS(frame), flags)) {
        printk_err("map fail page=0x%x flags=0x%x\n", page, flags);
        jlos_page_frame_free(frame);
        return false;
    }
    return true;
}

uint32_t jlos_vma_find_free_area(jlos_mm_t *mm, uint32_t base, uint32_t limit, uint32_t hint, uint32_t len)
{
    if (!mm || !len) {
        return 0;
    }
    if (hint >= base && hint < limit) {
        uint32_t h = JLOS_PAGE_ALIGN_UP(hint);
        if (h < limit && len <= limit - h) {
            jlos_rbtree_node_t *node = jlos_rbtree_find_key_le(&mm->vma_tree, &h, jlos_vma_cmp_addr);
            if (!node) {
                node = jlos_rbtree_first(&mm->vma_tree);
            } else {
                jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
                if (vma->end <= h) {
                    node = jlos_rbtree_next(&mm->vma_tree, node);
                }
            }
            jlos_vma_t *vma = node ? jlos_rbtree_entry(node, jlos_vma_t, rb_node) : NULL;
            if (!vma || vma->start >= h + len) {
                return h;
            }
        }
    }
    uint32_t candidate = base;
    jlos_rbtree_node_t *node = jlos_rbtree_find_key_le(&mm->vma_tree, &candidate, jlos_vma_cmp_addr);
    if (!node) {
        node = jlos_rbtree_first(&mm->vma_tree);
    } else {
        jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
        if (vma->end > candidate) {
            candidate = JLOS_PAGE_ALIGN_UP(vma->end);
        }
        node = jlos_rbtree_next(&mm->vma_tree, node);
    }
    while (node) {
        jlos_vma_t *vma = jlos_rbtree_entry(node, jlos_vma_t, rb_node);
        if (vma->start >= candidate && vma->start - candidate >= len) {
            break;
        }
        candidate = JLOS_PAGE_ALIGN_UP(vma->end);
        node = jlos_rbtree_next(&mm->vma_tree, node);
    }
    if (candidate < base || candidate >= limit || len > limit - candidate) {
        return 0;
    }
    return candidate;
}

static void jlos_vma_caches_init(void)
{
    s_vma_cache = jlos_memory_slab_cache_create("jlos_vma", sizeof(jlos_vma_t), sizeof(void *), 0, NULL, NULL);
    s_mm_cache = jlos_memory_slab_cache_create("jlos_mm", sizeof(jlos_mm_t), sizeof(void *), 0, NULL, NULL);
    s_pctx_cache = jlos_memory_slab_cache_create("jlos_pctx", sizeof(jlos_paging_context_t), sizeof(void *), 0, NULL, NULL);
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_vma_caches_init);
