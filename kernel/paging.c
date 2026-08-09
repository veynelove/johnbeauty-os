#include <hal/paging.h>
#include <hal/irq.h>
#include <hal/hal.h>
#include <hal/spinlock.h>
#include <kernel/paging.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/printk.h>

extern jlos_task_t *g_current_task_ptr;

jlos_paging_context_t *jlos_active_paging_context = NULL;
jlos_paging_context_t s_kernel_paging_context;

static jlos_spinlock_t s_paging_lock = JLOS_SPINLOCK_INIT;

void jlos_paging_context_init(jlos_paging_context_t *self)
{
    self->page_dir = NULL;
    self->num_page_tables = 0;
}

void jlos_paging_context_destroy(jlos_paging_context_t *self)
{
    if (!self->page_dir) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&s_paging_lock);
    for (uint32_t i = 0; i < JLOS_PAGE_DIR_ENTRIES; i++) {
        jlos_page_dir_entry_t *pde = &self->page_dir->entries[i];
        if (!(*pde & JLOS_PDE_PRESENT)) {
            continue;
        }
        if (*pde & JLOS_PDE_4MB) {
            uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
            for (uint32_t j = 0; j < 1024; j++) {
                jlos_page_frame_free((void *)PHYS_TO_VIRT(phys_base + j * JLOS_PAGE_SIZE));
            }
        } else {
            jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
            for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
                jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
                if (*pte & JLOS_PTE_PRESENT) {
                    jlos_page_frame_free((void *)PHYS_TO_VIRT((*pte) & JLOS_PAGE_ADDR_MASK));
                }
            }
            jlos_page_frame_free(pt);
        }
    }
    jlos_spin_unlock_irqrestore(&s_paging_lock, flags);
    jlos_page_frame_free(self->page_dir);
    jlos_paging_context_init(self);
}

bool jlos_paging_map(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    if (!self->page_dir) {
        self->page_dir = jlos_page_frame_malloc();
        if (!self->page_dir) {
            return false;
        }
        jlos_memset(self->page_dir, 0, sizeof(jlos_page_dir_t));
    }
    uint32_t fl = 0;
    if (flags & JLOS_PDE_4MB) {
        uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
        fl = jlos_spin_lock_irqsave(&s_paging_lock);
        jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
        *pde = (physical_addr & JLOS_PDE_4MB_ADDR_MASK) | flags | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE;
        jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
        jlos_hal_paging_flush_tlb(virtual_addr);
        return true;
    }
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);

    jlos_page_table_t *page_table = NULL;
    fl = jlos_spin_lock_irqsave(&s_paging_lock);
    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];

    if (*pde & JLOS_PDE_PRESENT) {
        page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
    } else {
        page_table = jlos_page_frame_malloc();
        if (!page_table) {
            jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
            return false;
        }
        jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
        *pde = VIRT_TO_PHYS((uint32_t)page_table) | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (flags & JLOS_PDE_USER);
        self->num_page_tables++;
    }
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    if (*pte & JLOS_PTE_PRESENT) {
        jlos_page_frame_free((void *)PHYS_TO_VIRT(*pte & JLOS_PAGE_ADDR_MASK));
    }
    *pte = (physical_addr & JLOS_PAGE_ADDR_MASK) | flags;
    jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
    jlos_hal_paging_flush_tlb(virtual_addr);
    return true;
}

bool jlos_paging_map_range(jlos_paging_context_t *self, uint32_t virtual_addr_start,
    uint32_t physical_addr_start, size_t size, uint32_t flags)
{
    if (!self->page_dir) {
        self->page_dir = jlos_page_frame_malloc();
        if (!self->page_dir) {
            return false;
        }
        jlos_memset(self->page_dir, 0, sizeof(jlos_page_dir_t));
    }
    uint32_t virtual_addr = virtual_addr_start;
    uint32_t physical_addr = physical_addr_start;
    uint32_t end_addr = virtual_addr_start + size;
    uint32_t pages_done = 0;
    
    uint32_t fl = jlos_spin_lock_irqsave(&s_paging_lock);
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    while (virtual_addr < end_addr) {
        uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);
        jlos_page_dir_entry_t *pde =  &self->page_dir->entries[pd_index];
        jlos_page_table_t *page_table = NULL;
        if (*pde & JLOS_PDE_PRESENT) {
            page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
        } else {
            page_table = jlos_page_frame_malloc();
            if (!page_table) {
                jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
                for (uint32_t i = 0; i < pages_done; i++) {
                    virtual_addr -= JLOS_PAGE_SIZE;
                    jlos_paging_unmap(self, virtual_addr);
                }
                return false;
            }
            jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
            *pde = VIRT_TO_PHYS((uint32_t)page_table) | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (flags & JLOS_PDE_USER);
            self->num_page_tables++;
        }
        jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
        *pte = (physical_addr & JLOS_PAGE_ADDR_MASK) | flags;
        virtual_addr += JLOS_PAGE_SIZE;
        physical_addr += JLOS_PAGE_SIZE;
        if ((virtual_addr & 0x3FFFFF) == 0) {
            pd_index++;
        }
        pages_done++;
    }
    jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
    /* TLB 刷新策略：
     * - 如果 self 不是当前 active 的 paging context（即正在构建新页表），
     *   完全不需要刷 TLB —— 这些映射在 CR3 切换时自然生效。
     * - 如果 self 就是 active context，用 HAL 刷全 TLB，
     *   绝对不能调用 jlos_hal_paging_switch 切到别的 page_dir！
     */
    if (self == jlos_active_paging_context) {
        uint32_t num_pages = JLOS_EXCEPT_CEIL(size, JLOS_PAGE_SIZE);
        if (num_pages <= 32) {
            for (uint32_t va = virtual_addr_start; va < virtual_addr_start + size; va += JLOS_PAGE_SIZE) {
                jlos_hal_paging_flush_tlb(va);
            }
        } else {
            jlos_hal_paging_flush_all_tlb();
        }
    }
    return true;
}

bool jlos_paging_unmap(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self->page_dir) {
        return false;
    }
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);

    uint32_t fl = jlos_spin_lock_irqsave(&s_paging_lock);
    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
        return false;
    }
    if (*pde & JLOS_PDE_4MB) {
        uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
        for (int i = 0; i < 1024; i++) {
            jlos_page_frame_free((void *)PHYS_TO_VIRT(phys_base + i * JLOS_PAGE_SIZE));
        }
        *pde = 0;
        jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
        jlos_hal_paging_flush_tlb(virtual_addr);
        return true;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    if (*pte & JLOS_PTE_PRESENT) {
        jlos_page_frame_free((void *)PHYS_TO_VIRT(*pte & JLOS_PAGE_ADDR_MASK));
    }
    *pte = 0;

    bool is_empty = true;
    for (uint32_t i = 0; i < JLOS_PAGE_TABLE_ENTRIES; i++) {
        if (page_table->entries[i]) {
            is_empty = false;
            break;
        }
    }
    if (is_empty) {
        jlos_page_frame_free(page_table);
        *pde = 0;
        self->num_page_tables--;
    }
    jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
    jlos_hal_paging_flush_tlb(virtual_addr);
    return true;
}

uint32_t jlos_paging_get_physical_addr(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self->page_dir) {
        return virtual_addr;
    }
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);

    uint32_t fl = jlos_spin_lock_irqsave(&s_paging_lock);
    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
        return 0;
    }
    if (*pde & JLOS_PDE_4MB) {
        jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
        uint32_t offset = virtual_addr & 0x3FFFFF;
        return (*pde & JLOS_PDE_4MB_ADDR_MASK) | offset;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
    uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
        return 0;
    }
    jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
    return ((*pte) & JLOS_PAGE_ADDR_MASK) | jlos_paging_get_page_offset(virtual_addr);
}

void jlos_paging_enable(jlos_paging_context_t *self)
{
    jlos_active_paging_context = self;
    jlos_hal_paging_enable(VIRT_TO_PHYS((uint32_t)self->page_dir));
}

void jlos_paging_disable(void)
{
    jlos_hal_paging_disable();
}

void jlos_paging_switch(jlos_paging_context_t *self)
{
    jlos_active_paging_context = self;
    jlos_hal_paging_switch(VIRT_TO_PHYS((uint32_t)self->page_dir));
}

void jlos_paging_change_flags_range(jlos_paging_context_t *self, uint32_t virtual_addr_start, uint32_t virtual_addr_end,
    uint32_t flags)
{
    uint32_t fl = jlos_spin_lock_irqsave(&s_paging_lock);
    uint32_t start = JLOS_PAGE_ALIGN_DOWN(virtual_addr_start);
    for (uint32_t addr = start; addr < virtual_addr_end; addr += JLOS_PAGE_SIZE) {
        uint32_t pd_idx = jlos_paging_get_page_dir_index(addr);
        uint32_t pt_idx = jlos_paging_get_page_table_index(addr);

        jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_idx];
        if (!(*pde & JLOS_PDE_PRESENT)) continue;
        if (*pde & JLOS_PDE_4MB) {
            /* FIX: 4MB PSE 页改 flags 时必须保留 PDE_4MB (PS) 位！否则下一次翻译会当成 PT 指针走 → PF */
            uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
            uint32_t keep = *pde & (JLOS_PDE_4MB | JLOS_PDE_GLOBAL | 0x00001000 /* PAT */ | 0x00000E00 /* PCD|PWT|A|D */);
            *pde = phys_base | flags | keep;
            jlos_hal_paging_flush_tlb(addr);
            continue;
        }
        jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
        jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
        if (!(*pte & JLOS_PTE_PRESENT)) continue;
        uint32_t phys = *pte & JLOS_PAGE_ADDR_MASK;
        *pte = phys | flags;
        if (flags & JLOS_PTE_USER) {
            *pde |= JLOS_PDE_USER;
        }
        jlos_hal_paging_flush_tlb(addr);
    }
    jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
}

void jlos_paging_initialize_kernel_paging(void)
{
    jlos_paging_context_init(&s_kernel_paging_context);

    /* 恒等映射 0~1MB：低内存 BIOS/VGA/GRUB multiboot_info */
    jlos_paging_map_range(&s_kernel_paging_context,
        0, 0, 0x00100000, JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE);

    /* 高半核：0xC0000000+ → PA, 覆盖全部物理内存 */
    jlos_paging_map_range(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE,
        0, KERNEL_MEMORY_PHYSICAL_END, JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE);

    /* .text 只读 */
    const jlos_hal_kernel_segments_t *segments = jlos_hal_get_kernel_segments();
    if (segments->text_start != 0) {
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->text_start, segments->text_end,
            JLOS_PTE_PRESENT);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->data_start, segments->data_end,
            JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->bss_start, segments->bss_end,
            JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE);
    }

    jlos_paging_enable(&s_kernel_paging_context);
}

bool jlos_paging_is_user_accessible(uint32_t virtual_addr, uint32_t len)
{
    /* 经典 access_ok：用户地址必须在 0~3GB */
    if (!s_kernel_paging_context.page_dir
        || virtual_addr >= KERNEL_VIRTUAL_BASE
        || virtual_addr + len > KERNEL_VIRTUAL_BASE) {
        return false;
    }
    uint32_t flag = jlos_spin_lock_irqsave(&s_paging_lock);
    uint32_t addr = JLOS_PAGE_ALIGN_DOWN(virtual_addr);
    uint32_t end = JLOS_PAGE_ALIGN_DOWN(virtual_addr + len - 1);
    for (; addr <= end; addr += JLOS_PAGE_SIZE) {
        uint32_t pd_idx = jlos_paging_get_page_dir_index(addr);
        jlos_page_dir_entry_t *pde = &s_kernel_paging_context.page_dir->entries[pd_idx];
        if (!(*pde & JLOS_PDE_PRESENT)) {
            continue;
        }
        if (*pde & JLOS_PDE_4MB) {
            if (!(*pde & JLOS_PDE_USER)) {
                jlos_spin_unlock_irqrestore(&s_paging_lock, flag);
                return false;
            }
            continue;
        }
        uint32_t pt_idx = jlos_paging_get_page_table_index(addr);
        jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
        jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
        if (!(*pte & JLOS_PTE_PRESENT)) {
            continue;
        }
        if (!(*pte & JLOS_PTE_USER)) {
            jlos_spin_unlock_irqrestore(&s_paging_lock, flag);
            return false;
        }
    }
    jlos_spin_unlock_irqrestore(&s_paging_lock, flag);
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
        printk("kernel page fault at 0x%x, error=0x%x\n", fault_addr, error_code);
        if (!present) {
            printk("page not present\n");
        } else {
            if (write) {
                printk("write protection violation\n");
            } else {
                printk("read protection violation\n");
            }
        }
        printk("page fault handler: unrecoverable error!\n");
        printk("instruction pointer: 0x%x, code segment: 0x%x\n", context->instruction_pointer, context->code_segment);
        printk("EFLAGS: 0x%x\n", context->flags);
        
        for (;;) {
            jlos_hal_halt();
        }
    }

    printk("user page fault. pid = %u, addr = 0x%x, present = %u, write = %u\n",
        g_current_task_ptr ? g_current_task_ptr->pid : 0, fault_addr, present, write);
    
    if (g_current_task_ptr) {
        g_current_task_ptr->exit_code = TASK_EXIT_PAGE_FAULT;
        JLOS_TASK_SET_TERMINATED(g_current_task_ptr, g_current_task_ptr->exit_code);
    }
}

void jlos_paging_print_states(jlos_paging_context_t *self)
{
    if (!self->page_dir) {
        printk("page context not initialized\n");
        return;
    }
    uint32_t mapped_pages = 0;
    uint32_t fl = jlos_spin_lock_irqsave(&s_paging_lock);
    for (uint32_t pd_index = 0; pd_index < JLOS_PAGE_DIR_ENTRIES; pd_index++) {
        jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
        if (*pde & JLOS_PDE_PRESENT) {
            if (*pde & JLOS_PDE_4MB) {
                mapped_pages += JLOS_PAGE_TABLE_ENTRIES;
            } else {
                jlos_page_table_t *page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
                for (uint32_t pt_index = 0; pt_index < JLOS_PAGE_TABLE_ENTRIES; pt_index++) {
                    if (page_table->entries[pt_index] & JLOS_PTE_PRESENT) {
                        mapped_pages++;
                    }
                }
            }
        }
    }
    jlos_spin_unlock_irqrestore(&s_paging_lock, fl);
    printk("page states:\n");
    printk("  - page directory: 0x%x\n", self->page_dir);
    printk("  - page tables: %u\n", self->num_page_tables);
    printk("  - mapped pages: %u (%u KB)\n", mapped_pages, mapped_pages * 4);
    printk("  - total physical memory: %u KB\n", jlos_page_frame_get_total() * 4);
    printk("  - free physical memory: %u KB\n", jlos_page_frame_get_free() * 4);
}
