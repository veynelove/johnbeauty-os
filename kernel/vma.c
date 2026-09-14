#include <kernel/vma.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "vma"

jlos_mm_t *jlos_mm_create(void)
{
    jlos_mm_t *mm = (jlos_mm_t *)jlos_kalloc(sizeof(jlos_mm_t));
    if (!mm) {
        return NULL;
    }
    mm->pc = (jlos_paging_context_t *)jlos_kalloc(sizeof(jlos_paging_context_t));
    if (!mm->pc) {
        jlos_kfree(mm);
        return NULL;
    }
    jlos_paging_context_init(mm->pc);
    jlos_list_init(&mm->vma_list);
    jlos_spinlock_init(&mm->lock);
    mm->brk_start = 0;
    mm->brk_end = 0;
    mm->brk_limit = 0;
    return mm;
}

void jlos_mm_destroy(jlos_mm_t *mm)
{
    if (!mm) {
        return;
    }
    jlos_vma_t *vma, *tmp;
    jlos_list_for_each_entry_safe(vma, tmp, &mm->vma_list, link) {
        jlos_list_del(&vma->link);
        jlos_kfree(vma);
    }
    if (mm->pc) {
        jlos_paging_context_destroy(mm->pc);
        jlos_kfree(mm->pc);
    }
    jlos_kfree(mm);
}

bool jlos_mm_clone_user(jlos_mm_t *dst, jlos_mm_t *src)
{
    if (!dst || !src || !dst->pc || !src->pc) {
        return false;
    }
    dst->brk_start = src->brk_start;
    dst->brk_end = src->brk_end;
    dst->brk_limit = src->brk_limit;
    jlos_vma_t *svma;
    jlos_list_for_each_entry(svma, &src->vma_list, link) {
        jlos_vma_t *dvma = (jlos_vma_t *)jlos_kalloc(sizeof(jlos_vma_t));
        if (!dvma) {
            return false;
        }
        *dvma = *svma;
        dvma->flags |= JLOS_VMA_COW;
        jlos_list_init(&dvma->link);
        jlos_list_add_tail(&dvma->link, &dst->vma_list);
        jlos_paging_cow_range(src->pc, dst->pc, svma->start, svma->end);
    }
    return true;
}

jlos_vma_t *jlos_vma_find(jlos_mm_t *mm, uint32_t addr)
{
    if (!mm) {
        return NULL;
    }
    jlos_vma_t *vma;
    jlos_list_for_each_entry(vma, &mm->vma_list, link) {
        if (addr >= vma->start && addr < vma->end) {
            return vma;
        }
    }
    return NULL;
}

jlos_vma_t *jlos_vma_add(jlos_mm_t *mm, uint32_t start, uint32_t end, uint32_t flags, jlos_vma_type_t type)
{
    if (!mm || end <= start) {
        return NULL;
    }
    jlos_vma_t *vma = (jlos_vma_t *)jlos_kalloc(sizeof(jlos_vma_t));
    if (!vma) {
        return NULL;
    }
    vma->start = JLOS_PAGE_ALIGN_DOWN(start);
    vma->end = JLOS_PAGE_ALIGN_UP(end);
    vma->flags = flags;
    vma->type = type;
    jlos_list_init(&vma->link);
    vma->file = NULL;
    vma->offset = 0;

    jlos_vma_t *pos;
    jlos_list_for_each_entry(pos, &mm->vma_list, link) {
        if (vma->start < pos->start) {
            jlos_list_add(&vma->link, pos->link.prev);
            return vma;
        }
    }
    jlos_list_add_tail(&vma->link, &mm->vma_list);
    return vma;
}

bool jlos_vma_remove_range(jlos_mm_t *mm, uint32_t start, uint32_t end)
{
    if (!mm || !mm->pc || end <= start) {
        return false;
    }
    start = JLOS_PAGE_ALIGN_DOWN(start);
    end = JLOS_PAGE_ALIGN_UP(end);
    jlos_vma_t *vma, *tmp;
    jlos_list_for_each_entry_safe(vma, tmp, &mm->vma_list, link) {
        if (vma->start >= start && vma->end <= end) {
            for (uint32_t i = vma->start; i < vma->end; i += JLOS_PAGE_FRAME_SIZE) {
                jlos_paging_unmap(mm->pc, i);
            }
            jlos_list_del(&vma->link);
            jlos_kfree(vma);
        }
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
    jlos_vma_t *vma;
    jlos_list_for_each_entry(vma, &mm->vma_list, link) {
        if (vma->type == type && vma->start == start) {
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
    jlos_vma_t *vma;
    jlos_list_for_each_entry(vma, &mm->vma_list, link) {
        if (vma->type != type) {
            continue;
        }
        if (new_end <= vma->start) {
            for (uint32_t i = vma->start; i < vma->end; i += JLOS_PAGE_FRAME_SIZE) {
                jlos_paging_unmap(mm->pc, i);
            }
            jlos_list_del(&vma->link);
            jlos_kfree(vma);
        } else if (new_end < vma->end) {
            for (uint32_t i = new_end; i < vma->end; i += JLOS_PAGE_FRAME_SIZE) {
                jlos_paging_unmap(mm->pc, i);
            }
            vma->end = new_end;
        }
        return true;
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
