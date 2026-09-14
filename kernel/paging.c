#include <hal/paging.h>
#include <hal/hal.h>
#include <kernel/paging.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/printk.h>
#include <kernel/device.h>
#include <kernel/multitask.h>

#define JLOS_KERNEL_LOG_SUBSYS "paging"

extern uint32_t _boot_end_phys;
extern jlos_task_t *g_current_task_ptr;

jlos_paging_context_t *jlos_active_paging_context = NULL;
jlos_paging_context_t s_kernel_paging_context;

static page_table_alloc_fn s_page_table_alloc = NULL;

void jlos_paging_context_init(jlos_paging_context_t *self)
{
    self->page_dir = NULL;
    self->num_page_tables = 0;
    jlos_spinlock_init(&self->lock);
}

void jlos_paging_context_destroy(jlos_paging_context_t *self)
{
    if (!self || !self->page_dir) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&self->lock);
    uint32_t pt_freed = 0;
    for (uint32_t i = 0; i < JLOS_PAGE_DIR_ENTRIES && pt_freed < self->num_page_tables; i++) {
        jlos_page_dir_entry_t *pde = &self->page_dir->entries[i];
        if (!(*pde & JLOS_PDE_PRESENT)) {
            continue;
        }
        uint32_t vir_addr = i << 22;
        bool is_kernel_space = (vir_addr >= KERNEL_VIRTUAL_BASE);

        if (*pde & JLOS_PDE_4MB) {
            if (!is_kernel_space) {
                uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
                jlos_page_frame_free_order((void *)PHYS_TO_VIRT(phys_base), JLOS_PFA_MAX_ORDER);
            }
            continue;
        }
        
        jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
        if (!is_kernel_space) {
            for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
                jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
                if (*pte & JLOS_PTE_PRESENT) {
                    jlos_page_frame_refcount_dec(*pte & JLOS_PAGE_ADDR_MASK);
                }
            }
        }
        jlos_page_frame_clear_owner_type(VIRT_TO_PHYS(pt));
        jlos_page_frame_free(pt);
        pt_freed++;
    }
    jlos_spin_unlock_irqrestore(&self->lock, flags);
    jlos_page_frame_free(self->page_dir);
    jlos_paging_context_init(self);
}

void jlos_paging_context_clone(jlos_paging_context_t *dst, jlos_paging_context_t *src)
{
    jlos_paging_context_init(dst);
    dst->page_dir = jlos_page_frame_malloc();
    if (!dst->page_dir) {
        return;
    }
    jlos_memset(dst->page_dir, 0, sizeof(jlos_page_dir_t));
    if (!src->page_dir) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&src->lock);
    dst->num_page_tables = 0;
    uint32_t kernel_pde_start = KERNEL_VIRTUAL_BASE >> 22;
    for (uint32_t i = kernel_pde_start; i < JLOS_PAGE_DIR_ENTRIES; i++) {
        jlos_page_dir_entry_t src_pde = src->page_dir->entries[i];
        if (!(src_pde & JLOS_PDE_PRESENT)) {
            continue;
        }
        if (src_pde & JLOS_PDE_4MB) {
            dst->page_dir->entries[i] = src_pde;
        } else {
            jlos_page_table_t *src_pt = (jlos_page_table_t *)PHYS_TO_VIRT(src_pde & JLOS_PAGE_ADDR_MASK);
            jlos_page_table_t *dst_pt = (jlos_page_table_t *)jlos_page_frame_malloc();
            if (!dst_pt) {
                /* OOM: 只能放弃当前 PDE, 前面已分配的 PT 会随 page_dir 释放回收 */
                continue;
            }
            jlos_memcpy(dst_pt, src_pt, sizeof(jlos_page_table_t));
            uint32_t dst_pt_phys = VIRT_TO_PHYS(dst_pt);
            /* 新 PDE = 原 PDE 低位标志 + 新 PT 物理地址 */
            dst->page_dir->entries[i] = (src_pde & ~JLOS_PAGE_ADDR_MASK) | dst_pt_phys;
            dst->num_page_tables++;
            uint32_t src_pt_phys = src_pde & JLOS_PAGE_ADDR_MASK;
            jlos_page_frame_set_owner_type(dst_pt_phys, NULL, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
            jlos_page_frame_pt_present_count_set(dst_pt_phys, jlos_page_frame_pt_present_count_get(src_pt_phys));
        }
    }
    jlos_spin_unlock_irqrestore(&src->lock, flags);
}

static bool jlos_paging_unmap_nolock(jlos_paging_context_t *self, uint32_t vir_addr)
{
    if (!self || !self->page_dir) {
        return false;
    }
    uint32_t pd_idx = jlos_paging_get_page_dir_index(vir_addr);
    uint32_t pt_idx = jlos_paging_get_page_table_index(vir_addr);
    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_idx];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return false;
    }
    if (*pde & JLOS_PDE_4MB) {
        uint32_t region_base = pd_idx << 22;
        if (region_base < KERNEL_VIRTUAL_BASE) {
            uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
            jlos_page_frame_free_bulk(phys_base, 1024);
        }
        *pde = 0;
        return true;
    }
    uint32_t pt_phys = (*pde) & JLOS_PAGE_ADDR_MASK;
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(pt_phys);
    jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
    if (*pte & JLOS_PTE_PRESENT) {
        jlos_page_frame_free((void *)PHYS_TO_VIRT(*pte & JLOS_PAGE_ADDR_MASK));
        jlos_page_frame_pt_present_count_dec(pt_phys);
    }
    *pte = 0;
    if (jlos_page_frame_pt_present_count_get(pt_phys) == 0) {
        uint32_t region_base = pd_idx << 22;
        jlos_page_frame_clear_owner_type(pt_phys);
        if (region_base < KERNEL_VIRTUAL_BASE) {
            jlos_page_frame_free(pt);
        }
        *pde = 0;
        self->num_page_tables--;
    }
    return true;
}

static bool jlos_paging_map_range_nolock(jlos_paging_context_t *self, uint32_t virtual_addr_start,
    uint32_t physical_addr_start, size_t size, uint32_t flags)
{
    if (!self->page_dir) {
        self->page_dir = s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
        if (!self->page_dir) {
            return false;
        }
        jlos_memset(self->page_dir, 0, sizeof(jlos_page_dir_t));
    }
    uint32_t virtual_addr = virtual_addr_start;
    uint32_t physical_addr = physical_addr_start;
    if (size && (virtual_addr_start + size < virtual_addr_start
        || physical_addr_start + size < physical_addr_start)) {
        return false;
    }
    uint32_t end_addr = virtual_addr_start + size;
    uint32_t pages_done = 0;
    
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    while (virtual_addr < end_addr) {
        uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);
        jlos_page_dir_entry_t *pde =  &self->page_dir->entries[pd_index];
        jlos_page_table_t *page_table = NULL;
        uint32_t pt_phys;
        if (*pde & JLOS_PDE_PRESENT) {
            page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
            pt_phys = (*pde) & JLOS_PAGE_ADDR_MASK;
        } else {
            page_table = s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
            if (!page_table) {
                for (uint32_t i = 0; i < pages_done; i++) {
                    virtual_addr -= JLOS_PAGE_SIZE;
                    jlos_paging_unmap_nolock(self, virtual_addr);
                }
                return false;
            }
            jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
            pt_phys = VIRT_TO_PHYS(page_table);
            *pde = VIRT_TO_PHYS(page_table) | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (flags & JLOS_PDE_USER);
            self->num_page_tables++;
            jlos_page_frame_set_owner_type(pt_phys, NULL, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
            jlos_page_frame_pt_present_count_set(pt_phys, 0);
        }
        jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
        if (*pte & JLOS_PTE_PRESENT) {
            printk_err("remap present pte. va = 0x%x, old = 0x%x\n", virtual_addr, *pte);
            goto halt;
        }
        *pte = (physical_addr & JLOS_PAGE_ADDR_MASK) | flags;
        jlos_page_frame_pt_present_count_inc(pt_phys);
        virtual_addr += JLOS_PAGE_SIZE;
        physical_addr += JLOS_PAGE_SIZE;
        if ((virtual_addr & 0x3FFFFF) == 0) {
            pd_index++;
        }
        pages_done++;
    }
    return true;
halt:
    for (;;) {
        jlos_hal_halt();
    }
}

static bool jlos_paging_map_nolock(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    if (!self) {
        return false;
    }
    if (!self->page_dir) {
        self->page_dir = s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
        if (!self->page_dir) {
            return false;
        }
        jlos_memset(self->page_dir, 0, sizeof(jlos_page_dir_t));
    }
    if (flags & JLOS_PDE_4MB) {
        uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
        jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
        if ((*pde & JLOS_PDE_PRESENT) && !(*pde & JLOS_PDE_4MB)) {
            jlos_page_table_t *old_pt = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
            for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
                if (old_pt->entries[pt_idx] & JLOS_PTE_PRESENT) {
                    jlos_page_frame_free((void *)PHYS_TO_VIRT(old_pt->entries[pt_idx] & JLOS_PAGE_ADDR_MASK));
                }
            }
            jlos_page_frame_clear_owner_type(VIRT_TO_PHYS(old_pt));
            jlos_page_frame_free(old_pt);
            self->num_page_tables--;
        }
        *pde = (physical_addr & JLOS_PDE_4MB_ADDR_MASK) | flags | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE;
        return true;
    }
    return jlos_paging_map_range_nolock(self, virtual_addr, physical_addr, JLOS_PAGE_SIZE, flags);
}

static uint32_t jlos_paging_get_physical_addr_nolock(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self || !self->page_dir) {
        return virtual_addr;
    }
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return 0;
    }
    if (*pde & JLOS_PDE_4MB) {
        uint32_t offset = virtual_addr & 0x3FFFFF;
        return (*pde & JLOS_PDE_4MB_ADDR_MASK) | offset;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
    uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    return ((*pte) & JLOS_PAGE_ADDR_MASK) | jlos_paging_get_page_offset(virtual_addr);
}

static void jlos_paging_change_flags_nolock(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t flags)
{
    uint32_t pd_idx = jlos_paging_get_page_dir_index(virtual_addr);
    uint32_t pt_idx = jlos_paging_get_page_table_index(virtual_addr);

    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_idx];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return;
    }
    if (*pde & JLOS_PDE_4MB) {
        uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
        uint32_t keep = *pde & (JLOS_PDE_4MB | JLOS_PDE_GLOBAL | 0x00001000 |
            JLOS_PDE_WRITE_THROUGH | JLOS_PDE_CACHE_DISABLE | JLOS_PDE_ACCESSED | JLOS_PDE_DIRTY);
        *pde = phys_base | flags | keep;
        return;
    }
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
    jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return;
    }
    uint32_t phys = *pte & JLOS_PAGE_ADDR_MASK;
    uint32_t preserve = *pte & (JLOS_PTE_GLOBAL |
        JLOS_PTE_DIRTY | JLOS_PTE_CACHE_DISABLE | JLOS_PTE_WRITE_THROUGH | JLOS_PTE_ACCESSED);
    
    *pte = phys | flags | preserve;

    if (flags & JLOS_PTE_USER) {
        *pde |= JLOS_PDE_USER;
    }
}

bool jlos_paging_map(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    if (!self) {
        return false;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    bool ret = jlos_paging_map_nolock(self, virtual_addr, physical_addr, flags);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (ret && self == jlos_active_paging_context) {
        jlos_hal_paging_flush_tlb(virtual_addr);
    }
    return ret;
}

bool jlos_paging_map_range(jlos_paging_context_t *self, uint32_t virtual_addr_start,
    uint32_t physical_addr_start, size_t size, uint32_t flags)
{   
    if (!self || !self->page_dir) {
        return false;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    bool ret = jlos_paging_map_range_nolock(self, virtual_addr_start, physical_addr_start, size, flags);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (ret && self == jlos_active_paging_context) {
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
    if (!self || !self->page_dir) {
        return false;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    bool ret = jlos_paging_unmap_nolock(self, virtual_addr);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (ret && self == jlos_active_paging_context) {
        jlos_hal_paging_flush_tlb(virtual_addr);
    }
    return ret;
}

uint32_t jlos_paging_get_physical_addr(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self || !self->page_dir) {
        return virtual_addr;
    }

    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    uint32_t phys = jlos_paging_get_physical_addr_nolock(self, virtual_addr);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    return phys;
}

extern jlos_paging_context_t s_kernel_paging_context;

void jlos_paging_enable(jlos_paging_context_t *self)
{
    jlos_active_paging_context = self;
    jlos_hal_paging_enable(VIRT_TO_PHYS(self->page_dir));
}

void jlos_paging_switch(jlos_paging_context_t *self)
{
    jlos_active_paging_context = self;
    jlos_hal_paging_switch(VIRT_TO_PHYS(self->page_dir));
}

void jlos_paging_change_flags(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t flags)
{
    if (!self || !self->page_dir) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    jlos_paging_change_flags_nolock(self, virtual_addr, flags);
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    if (self == jlos_active_paging_context) {
        jlos_hal_paging_flush_tlb(virtual_addr);
    }
}

void jlos_paging_change_flags_range(jlos_paging_context_t *self, uint32_t virtual_addr_start, uint32_t virtual_addr_end,
    uint32_t flags)
{
    if (!self || !self->page_dir || virtual_addr_end <= virtual_addr_start) {
        return;
    }
    uint32_t start = JLOS_PAGE_ALIGN_DOWN(virtual_addr_start);
    uint32_t page_num = (virtual_addr_end - start) / JLOS_PAGE_SIZE;
    bool active = (self == jlos_active_paging_context);
    bool flush_one = active && page_num < JLOS_PAGE_FRAME_FLUSH_ALL_TLB_THRESHOLD;

    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    for (uint32_t addr = start; addr < virtual_addr_end; addr += JLOS_PAGE_SIZE) {
        uint32_t pd_idx = jlos_paging_get_page_dir_index(addr);
        jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_idx];
        if (!(*pde & JLOS_PDE_PRESENT)) {
            addr = ((pd_idx + 1) << 22) - JLOS_PAGE_SIZE;
            continue;
        }
        jlos_paging_change_flags_nolock(self, addr, flags);
        if (flush_one) {
            jlos_hal_paging_flush_tlb(addr);
        }
    }
    if (active && page_num >= JLOS_PAGE_FRAME_FLUSH_ALL_TLB_THRESHOLD) {
        jlos_hal_paging_flush_all_tlb();
    }
    jlos_spin_unlock_irqrestore(&self->lock, fl);
}

bool jlos_paging_cow_range(jlos_paging_context_t *src, jlos_paging_context_t *dst, uint32_t start, uint32_t end)
{
    if (!src || !dst || !src->page_dir || !dst->page_dir || end <= start) {
        return false;
    }
    start = JLOS_PAGE_ALIGN_DOWN(start);
    end = JLOS_PAGE_ALIGN_UP(end);
    uint32_t src_fl = jlos_spin_lock_irqsave(&src->lock);
    uint32_t dst_fl = jlos_spin_lock_irqsave(&dst->lock);
    for (uint32_t va = start; va < end; va += JLOS_PAGE_SIZE) {
        uint32_t pd_idx = jlos_paging_get_page_dir_index(va);
        jlos_page_dir_entry_t src_pde = src->page_dir->entries[pd_idx];
        if (!(src_pde & JLOS_PDE_PRESENT)) {
            va = ((pd_idx + 1) << 22) - JLOS_PAGE_SIZE;
            continue;
        }
        if (src_pde & JLOS_PDE_4MB) {
            continue;
        }
        uint32_t pt_phys = src_pde & JLOS_PAGE_ADDR_MASK;
        jlos_page_table_t *src_pt = (jlos_page_table_t *)PHYS_TO_VIRT(pt_phys);
        uint32_t pt_idx = jlos_paging_get_page_table_index(va);
        jlos_page_table_entry_t src_pte = src_pt->entries[pt_idx];
        if (!(src_pte & JLOS_PTE_PRESENT)) {
            continue;
        }
        uint32_t phys = src_pte & JLOS_PAGE_ADDR_MASK;
        jlos_page_frame_refcount_inc(phys);
        jlos_paging_map_nolock(dst, va, phys, JLOS_PTE_USER_COW);
        src_pt->entries[pt_idx] = src_pte & ~JLOS_PTE_WRITABLE;
    }
    if (src == jlos_active_paging_context) {
        jlos_hal_paging_flush_all_tlb();
    }
    jlos_spin_unlock_irqrestore(&dst->lock, dst_fl);
    jlos_spin_unlock_irqrestore(&src->lock, src_fl);
    return true;
}

void jlos_paging_initialize_kernel_paging(page_table_alloc_fn alloc_fn)
{
    s_page_table_alloc = alloc_fn;
    jlos_paging_context_init(&s_kernel_paging_context);
    
    /* 高半核：0xC0000000+ → PA, 覆盖全部物理内存 (限制在 1GB 内核空间内) */
    uint32_t map_size = jlos_device_physical_memory_end;
    if (map_size > KERNEL_DIRECT_MAP_SIZE) {
        map_size = KERNEL_DIRECT_MAP_SIZE;
    }
    uint32_t low_end = (uint32_t)&_boot_end_phys;
    uint32_t huge_start = JLOS_EXCEPT_CEIL(low_end, 4 * 1024 * 1024);
    if (huge_start > map_size) {
        huge_start = map_size;
    }
    if (huge_start) {
        jlos_paging_map_range(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE, 0, huge_start, JLOS_PTE_KERNEL_RW);
    }
    /* .text 只读 */
    const jlos_hal_kernel_segments_t *segments = jlos_hal_get_kernel_segments();
    if (segments->text_start) {
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->text_start, segments->text_end,
            JLOS_PTE_KERNEL_RO);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->data_start, segments->data_end,
            JLOS_PTE_KERNEL_RW);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->bss_start, segments->bss_end,
            JLOS_PTE_KERNEL_RW);
    }
    for (uint32_t pa = huge_start; pa < map_size; pa += 4 * 1024 * 1024) {
        jlos_paging_map(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE + pa, pa, JLOS_PTE_KERNEL_RW | JLOS_PDE_4MB);
    }
    jlos_paging_enable(&s_kernel_paging_context);
    s_page_table_alloc = NULL;
}

bool jlos_paging_is_user_accessible(jlos_paging_context_t *ctx, uint32_t virtual_addr, uint32_t len)
{
    /* 经典 access_ok：用户地址必须在 0~3GB */
    if (!ctx || !ctx->page_dir) {
        return false;
    }
    if (virtual_addr >= KERNEL_VIRTUAL_BASE || virtual_addr + len > KERNEL_VIRTUAL_BASE) {
        return false;
    }
    return true;
}

void jlos_paging_page_fault_handler(jlos_irq_context_t *context)
{
    extern jlos_paging_context_t s_kernel_paging_context;
    uint32_t fault_addr = jlos_hal_paging_get_fault_addr();
    uint32_t error_code = context->error;
    bool present = error_code & 0x01;
    bool write = error_code & 0x02;
    bool user = error_code & 0x04;
    if (!user) {
        for (;;) {
            jlos_hal_halt();
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
        uint32_t old_phys = jlos_paging_get_physical_addr_nolock(ctx, page_addr);
        if (old_phys) {
            uint8_t refcount = jlos_page_frame_refcount_get(old_phys);
            if (refcount <= 1) {
                jlos_paging_change_flags_nolock(ctx, page_addr, JLOS_PTE_USER_RW);
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
            uint32_t cow_pd = jlos_paging_get_page_dir_index(page_addr);
            uint32_t cow_pt = jlos_paging_get_page_table_index(page_addr);
            jlos_page_dir_entry_t *cow_pde = &ctx->page_dir->entries[cow_pd];
            if ((*cow_pde & JLOS_PDE_PRESENT) && !(*cow_pde & JLOS_PDE_4MB)) {
                jlos_page_table_t *cow_ptbl = (jlos_page_table_t *)PHYS_TO_VIRT(*cow_pde & JLOS_PAGE_ADDR_MASK);
                jlos_page_frame_refcount_dec(old_phys);
                cow_ptbl->entries[cow_pt] = (VIRT_TO_PHYS(new_frame) & JLOS_PAGE_ADDR_MASK) | JLOS_PTE_USER_RW;
            } else {
                jlos_paging_unmap_nolock(ctx, page_addr);
                if (!jlos_paging_map_nolock(ctx, page_addr, VIRT_TO_PHYS(new_frame), JLOS_PTE_USER_RW)) {
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
    printk_err("user page fault. pid = %u, addr = 0x%x, present = %u, write = %u, ins_pointer = 0x%x\n",
        task->pid, fault_addr, present, write, context->instruction_pointer);
    jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
page_fault_oom:
    printk_err("page fault oom. pid = %u, addr = 0x%x\n", task->pid, fault_addr);
    jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
}

void jlos_paging_print_states(jlos_paging_context_t *self)
{
    if (!self->page_dir) {
        printk_info("page context not initialized\n");
        return;
    }
    uint32_t mapped_pages = 0;
    uint32_t fl = jlos_spin_lock_irqsave(&self->lock);
    for (uint32_t pd_index = 0; pd_index < JLOS_PAGE_DIR_ENTRIES; pd_index++) {
        jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
        if (*pde & JLOS_PDE_PRESENT) {
            if (*pde & JLOS_PDE_4MB) {
                mapped_pages += JLOS_PAGE_TABLE_ENTRIES;
            } else {
                uint32_t pt_phys = *pde & JLOS_PAGE_ADDR_MASK;
                mapped_pages += jlos_page_frame_pt_present_count_get(pt_phys);
            }
        }
    }
    jlos_spin_unlock_irqrestore(&self->lock, fl);
    printk_info("page states:\n");
    printk_info("- page directory: 0x%x\n", self->page_dir);
    printk_info("- page tables: %u\n", self->num_page_tables);
    printk_info("- mapped pages: %u (%u KB)\n", mapped_pages, mapped_pages * 4);
    printk_info("- total physical memory: %u KB\n", jlos_page_frame_get_total() * 4);
    printk_info("- free physical memory: %u KB\n", jlos_page_frame_get_free() * 4);
}
