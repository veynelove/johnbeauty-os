#include <hal/paging.h>
#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/multitask.h>
#include <kernel/vma.h>

#define JLOS_KERNEL_LOG_SUBSYS "paging"
#include <kernel/printk.h>

extern jlos_task_t          *g_current_task_ptr;

void jlos_paging_context_init(jlos_paging_context_t *self)
{
    self->root = NULL;
    self->num_page_tables = 0;
    jlos_spinlock_init(&self->lock);
}

void jlos_paging_context_destroy(jlos_paging_context_t *self)
{
    jlos_arch_paging_context_tables_destroy(self);
}

void jlos_paging_context_clone(jlos_paging_context_t *dst, jlos_paging_context_t *src)
{
    jlos_arch_paging_context_tables_clone(dst, src);
}

bool jlos_paging_map(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t physical_addr, uint32_t prot)
{
    if (!self) {
        return false;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    bool ret = jlos_arch_map_entry(self, virtual_addr, physical_addr, prot);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (ret && self == jlos_hal_paging_get_active_context()) {
        jlos_hal_paging_flush_tlb(virtual_addr);
    }
    return ret;
}

bool jlos_paging_map_range(jlos_paging_context_t *self, uint32_t virtual_addr_start,
    uint32_t physical_addr_start, size_t size, uint32_t prot)
{
    if (!self) {
        return false;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    bool ret = jlos_arch_map_range_entry(self, virtual_addr_start, physical_addr_start, size, prot);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (ret && self == jlos_hal_paging_get_active_context()) {
        uint32_t num_pages = JLOS_EXCEPT_CEIL(size, JLOS_PAGE_SIZE);
        if (num_pages < JLOS_PAGE_FRAME_FLUSH_ALL_TLB_THRESHOLD) {
            for (uint32_t va = virtual_addr_start; va < virtual_addr_start + size; va += JLOS_PAGE_SIZE) {
                jlos_hal_paging_flush_tlb(va);
            }
        } else {
            jlos_hal_paging_flush_all_tlb();
        }
    }
    return ret;
}

bool jlos_paging_unmap(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self || !self->root) {
        return false;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    bool ret = jlos_arch_unmap_entry(self, virtual_addr);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (ret && self == jlos_hal_paging_get_active_context()) {
        jlos_hal_paging_flush_tlb(virtual_addr);
    }
    return ret;
}

uint32_t jlos_paging_get_physical_addr(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self || !self->root) {
        return virtual_addr;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    uint32_t phys = jlos_arch_get_phys_entry(self, virtual_addr);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    return phys;
}

void jlos_paging_change_flags(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t prot)
{
    if (!self || !self->root) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    jlos_arch_change_flags_entry(self, virtual_addr, prot);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (jlos_arch_need_flush_tlb(self, virtual_addr)) {
        jlos_hal_paging_flush_tlb(virtual_addr);
    }
}

void jlos_paging_change_flags_range(jlos_paging_context_t *self, uint32_t virtual_addr_start, uint32_t virtual_addr_end,
    uint32_t prot)
{
    if (!self || !self->root || virtual_addr_end <= virtual_addr_start) {
        return;
    }
    uint32_t start = JLOS_PAGE_ALIGN_DOWN(virtual_addr_start);
    uint32_t page_num = (virtual_addr_end - start) / JLOS_PAGE_SIZE;
    bool flush_one = page_num < JLOS_PAGE_FRAME_FLUSH_ALL_TLB_THRESHOLD;
    bool need_flush_all = false;

    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    for (uint32_t addr = start; addr < virtual_addr_end; addr += JLOS_PAGE_SIZE) {
        jlos_arch_change_flags_entry(self, addr, prot);
        if (jlos_arch_need_flush_tlb(self, addr)) {
            if (flush_one) {
                jlos_hal_paging_flush_tlb(addr);
            } else {
                need_flush_all = true;
            }
        }
    }
    if (need_flush_all) {
        jlos_hal_paging_flush_all_tlb();
    }
    jlos_spin_unlock_irqrestore(&self->lock, fl);
}

bool jlos_paging_cow_range(jlos_paging_context_t *src, jlos_paging_context_t *dst, uint32_t start, uint32_t end)
{
    if (!src || !dst || !src->root || end <= start || src == dst) {
        return false;
    }
    start = JLOS_PAGE_ALIGN_DOWN(start);
    end = JLOS_PAGE_ALIGN_UP(end);
    uint32_t src_fl = jlos_spin_lock_irqsave(&src->lock);
    uint32_t dst_fl = jlos_spin_lock_irqsave(&dst->lock);
    for (uint32_t va = start; va < end; va += JLOS_PAGE_SIZE) {
        if (!jlos_arch_pte_present(src, va)) {
            continue;
        }
        uint32_t phys = jlos_arch_pte_phys(src, va);
        jlos_page_frame_refcount_inc(phys);
        jlos_arch_map_entry(dst, va, phys, JLOS_PG_USER_RO);
        jlos_arch_pte_make_ro(src, va);
    }
    if (src == jlos_hal_paging_get_active_context()) {
        jlos_hal_paging_flush_all_tlb();
    }
    jlos_spin_unlock_irqrestore(&dst->lock, dst_fl);
    jlos_spin_unlock_irqrestore(&src->lock, src_fl);
    return true;
}

void jlos_paging_enable(jlos_paging_context_t *self)
{
    jlos_hal_paging_set_active_context(self);
    jlos_hal_paging_enable(VIRT_TO_PHYS(self->root));
}

void jlos_paging_switch(jlos_paging_context_t *self)
{
    jlos_hal_paging_set_active_context(self);
    jlos_hal_paging_switch(VIRT_TO_PHYS(self->root));
}

void jlos_paging_initialize_kernel_paging(jlos_paging_table_alloc_fn alloc_fn)
{
    jlos_arch_paging_initialize_kernel(alloc_fn);
}

bool jlos_paging_is_user_accessible(jlos_paging_context_t *ctx, uint32_t virtual_addr, uint32_t len)
{
    /* 经典 access_ok：用户地址必须在 0~3GB */
    if (!ctx || !ctx->root) {
        return false;
    }
    if (virtual_addr >= KERNEL_VIRTUAL_BASE || virtual_addr + len > KERNEL_VIRTUAL_BASE) {
        return false;
    }
    return true;
}

void jlos_paging_page_fault_handler(jlos_irq_context_t *context)
{
    uint32_t fault_addr = jlos_hal_paging_get_fault_addr();
    uint32_t error_code = context->error;
    bool present = error_code & 0x01;
    bool write = error_code & 0x02;
    bool user = error_code & 0x04;
    if (!user) {
        if (fault_addr < KERNEL_VIRTUAL_BASE && g_current_task_ptr && g_current_task_ptr->mm) {
            user = true;
        } else {
            for (;;) {
                jlos_hal_halt();
            }
        }
    }

    if (!g_current_task_ptr || !g_current_task_ptr->mm) {
        for (;;) {
            jlos_hal_halt();
        }
    }

    jlos_task_t *task = g_current_task_ptr;
    jlos_paging_context_t *ctx = task->mm->pc;

    if (!present) {
        if (jlos_vma_demand_map(task->mm, fault_addr)) {
            return;
        }
        goto page_fault_kill;
    }
    if (present && write && fault_addr < KERNEL_VIRTUAL_BASE) {
        uint32_t page_addr = JLOS_PAGE_ALIGN_DOWN(fault_addr);
        uint32_t fl = jlos_spin_lock_irqsave(&ctx->lock);
        uint32_t old_phys = jlos_arch_get_phys_entry(ctx, page_addr);
        if (old_phys) {
            uint8_t refcount = jlos_page_frame_refcount_get(old_phys);
            if (refcount <= 1) {
                jlos_arch_change_flags_entry(ctx, page_addr, JLOS_PG_USER_RW);
                jlos_spin_unlock_irqrestore(&ctx->lock, fl);
                jlos_hal_paging_flush_tlb(page_addr);
                return;
            }
            void *new_frame = jlos_page_frame_malloc();
            if (!new_frame) {
                jlos_spin_unlock_irqrestore(&ctx->lock, fl);
                goto page_fault_oom;
            }
            jlos_memcpy(new_frame, (void *)PHYS_TO_VIRT(old_phys), JLOS_PAGE_FRAME_SIZE);

            if (jlos_arch_pte_replace(ctx, page_addr, VIRT_TO_PHYS(new_frame), JLOS_PG_USER_RW)) {
                jlos_page_frame_refcount_dec(old_phys);
            } else {
                jlos_arch_unmap_entry(ctx, page_addr);
                if (!jlos_arch_map_entry(ctx, page_addr, VIRT_TO_PHYS(new_frame), JLOS_PG_USER_RW)) {
                    jlos_page_frame_free(new_frame);
                    jlos_spin_unlock_irqrestore(&ctx->lock, fl);
                    goto page_fault_kill;
                }
            }
            jlos_spin_unlock_irqrestore(&ctx->lock, fl);
            jlos_hal_paging_flush_tlb(page_addr);
            return;
        }
    }

page_fault_kill:
    if (task->signal_handlers[JLOS_SIGSEGV] != 0 && task->signal_handlers[JLOS_SIGSEGV] != 1) {
        task->signal_pending |= (1U << JLOS_SIGSEGV);
        return;
    }
    printk_err("user page fault. pid = %u, addr = 0x%x, present = %u, write = %u, ins_pointer = 0x%x\n",
        task->pid, fault_addr, present, write, context->instruction_pointer);
    jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
page_fault_oom:
    printk_err("page fault oom. pid = %u, addr = 0x%x\n", task->pid, fault_addr);
    jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
}
