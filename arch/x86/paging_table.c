#include <hal/paging.h>
#include <hal/hal.h>
#include <hal/mmu.h>
#include <arch/x86/pgtable.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/device.h>

#define JLOS_KERNEL_LOG_SUBSYS "paging"
#include <kernel/printk.h>

extern uint32_t             _boot_end_phys;

jlos_paging_context_t       s_kernel_paging_context;
static jlos_paging_table_alloc_fn s_page_table_alloc = NULL;

static uint32_t pte_prot(uint32_t prot)
{
    uint32_t p = JLOS_PTE_PRESENT;
    if (prot & JLOS_PG_WRITE) {
        p |= JLOS_PTE_WRITABLE;
    }
    if (prot & JLOS_PG_USER) {
        p |= JLOS_PTE_USER;
    }
    if (prot & JLOS_PG_GLOBAL) {
        p |= JLOS_PTE_GLOBAL;
    }
    return p;
}

static jlos_page_table_entry_t *pte_walk(jlos_paging_context_t *ctx, uint32_t va)
{
    if (!ctx || !ctx->root) {
        return NULL;
    }
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT) || (*pde & JLOS_PDE_4MB)) {
        return NULL;
    }
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGE_ADDR_MASK);
    return &pt->entries[jlos_paging_get_page_table_index(va)];
}

static bool root_ensure(jlos_paging_context_t *ctx)
{
    if (ctx->root) {
        return true;
    }
    ctx->root = s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
    if (!ctx->root) {
        return false;
    }
    jlos_memset(ctx->root, 0, sizeof(jlos_page_dir_t));
    return true;
}

bool jlos_arch_pte_present(jlos_paging_context_t *ctx, uint32_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    return pte && (*pte & JLOS_PTE_PRESENT);
}

uint32_t jlos_arch_pte_phys(jlos_paging_context_t *ctx, uint32_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    return *pte & JLOS_PAGE_ADDR_MASK;
}

void jlos_arch_pte_set(jlos_paging_context_t *ctx, uint32_t va, uint32_t pa, uint32_t prot)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (pte) {
        *pte = (pa & JLOS_PAGE_ADDR_MASK) | pte_prot(prot);
    }
}

void jlos_arch_pte_set_rw(jlos_paging_context_t *ctx, uint32_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (pte && (*pte & JLOS_PTE_PRESENT)) {
        *pte |= JLOS_PTE_WRITABLE;
    }
}

void jlos_arch_pte_make_ro(jlos_paging_context_t *ctx, uint32_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (pte) {
        *pte &= ~JLOS_PTE_WRITABLE;
    }
}

bool jlos_arch_pte_replace(jlos_paging_context_t *ctx, uint32_t va, uint32_t pa, uint32_t prot)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_PTE_PRESENT)) {
        return false;
    }
    *pte = (pa & JLOS_PAGE_ADDR_MASK) | pte_prot(prot);
    return true;
}

static bool map_4mb_locked(jlos_paging_context_t *ctx, uint32_t va, uint32_t pa, uint32_t pte_flags)
{
    uint32_t pd_index = jlos_paging_get_page_dir_index(va);
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[pd_index];
    if ((*pde & JLOS_PDE_PRESENT) && !(*pde & JLOS_PDE_4MB)) {
        jlos_page_table_t *old_pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGE_ADDR_MASK);
        for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
            if (old_pt->entries[pt_idx] & JLOS_PTE_PRESENT) {
                jlos_page_frame_free((void *)PHYS_TO_VIRT(old_pt->entries[pt_idx] & JLOS_PAGE_ADDR_MASK));
            }
        }
        jlos_page_frame_clear_owner_type(VIRT_TO_PHYS(old_pt));
        jlos_page_frame_free(old_pt);
        ctx->num_page_tables--;
    }
    *pde = (pa & JLOS_PDE_4MB_ADDR_MASK) | pte_flags | JLOS_PDE_4MB | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE;
    return true;
}

bool jlos_arch_map_entry(jlos_paging_context_t *ctx, uint32_t va, uint32_t pa, uint32_t prot)
{
    if (!ctx) {
        return false;
    }
    if (!root_ensure(ctx)) {
        return false;
    }
    uint32_t pt_index = jlos_paging_get_page_table_index(va);
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[jlos_paging_get_page_dir_index(va)];
    jlos_page_table_t *page_table = NULL;
    uint32_t pt_phys;
    if (*pde & JLOS_PDE_PRESENT) {
        page_table = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGE_ADDR_MASK);
        pt_phys = *pde & JLOS_PAGE_ADDR_MASK;
    } else {
        page_table = s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
        if (!page_table) {
            printk_err("alloc page_table failed\n");
            return false;
        }
        jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
        pt_phys = VIRT_TO_PHYS(page_table);
        *pde = VIRT_TO_PHYS(page_table) | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (pte_prot(prot) & JLOS_PDE_USER);
        ctx->num_page_tables++;
        jlos_page_frame_set_owner_type(pt_phys, NULL, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
        jlos_page_frame_pt_present_count_set(pt_phys, 0);
    }
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    if (*pte & JLOS_PTE_PRESENT) {
        printk_err("remap present pte. va = 0x%x, old = 0x%x\n", va, *pte);
        return false;
    }
    *pte = (pa & JLOS_PAGE_ADDR_MASK) | pte_prot(prot);
    jlos_page_frame_pt_present_count_inc(pt_phys);
    return true;
}

static bool unmap_locked(jlos_paging_context_t *ctx, uint32_t va)
{
    if (!ctx || !ctx->root) {
        return false;
    }
    uint32_t pd_idx = jlos_paging_get_page_dir_index(va);
    uint32_t pt_idx = jlos_paging_get_page_table_index(va);
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[pd_idx];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return false;
    }
    if (*pde & JLOS_PDE_4MB) {
        uint32_t region_base = pd_idx << 22;
        if (region_base < KERNEL_VIRTUAL_BASE) {
            uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
            jlos_page_frame_free_n((void *)PHYS_TO_VIRT(phys_base), 1024);
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
        ctx->num_page_tables--;
    }
    return true;
}

bool jlos_arch_unmap_entry(jlos_paging_context_t *ctx, uint32_t va)
{
    return unmap_locked(ctx, va);
}

bool jlos_arch_map_range_entry(jlos_paging_context_t *ctx, uint32_t va_start, uint32_t pa_start,
    size_t size, uint32_t prot)
{
    if (!ctx || !root_ensure(ctx)) {
        return false;
    }
    uint32_t virtual_addr = va_start;
    uint32_t physical_addr = pa_start;
    if (size && (va_start + size < va_start || pa_start + size < pa_start)) {
        return false;
    }
    uint32_t end_addr = va_start + size;
    uint32_t pages_done = 0;

    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    while (virtual_addr < end_addr) {
        uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);
        jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[pd_index];
        jlos_page_table_t *page_table = NULL;
        uint32_t pt_phys;
        if (*pde & JLOS_PDE_PRESENT) {
            page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
            pt_phys = (*pde) & JLOS_PAGE_ADDR_MASK;
        } else {
            page_table = s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
            if (!page_table) {
                printk_err("alloc page_table failed\n");
                goto rollback;
            }
            jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
            pt_phys = VIRT_TO_PHYS(page_table);
            *pde = VIRT_TO_PHYS(page_table) | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (pte_prot(prot) & JLOS_PDE_USER);
            ctx->num_page_tables++;
            jlos_page_frame_set_owner_type(pt_phys, NULL, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
            jlos_page_frame_pt_present_count_set(pt_phys, 0);
        }
        jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
        if (*pte & JLOS_PTE_PRESENT) {
            printk_err("remap present pte. va = 0x%x, old = 0x%x\n", virtual_addr, *pte);
            goto rollback;
        }
        *pte = (physical_addr & JLOS_PAGE_ADDR_MASK) | pte_prot(prot);
        jlos_page_frame_pt_present_count_inc(pt_phys);
        virtual_addr += JLOS_PAGE_SIZE;
        physical_addr += JLOS_PAGE_SIZE;
        if ((virtual_addr & (JLOS_PDE_4MB_SIZE - 1)) == 0) {
            pd_index++;
        }
        pages_done++;
    }
    return true;
rollback:
    for (uint32_t i = 0; i < pages_done; i++) {
        virtual_addr -= JLOS_PAGE_SIZE;
        unmap_locked(ctx, virtual_addr);
    }
    return false;
}

uint32_t jlos_arch_get_phys_entry(jlos_paging_context_t *ctx, uint32_t va)
{
    if (!ctx || !ctx->root) {
        return va;
    }
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return 0;
    }
    if (*pde & JLOS_PDE_4MB) {
        uint32_t offset = va & (JLOS_PDE_4MB_SIZE - 1);
        return (*pde & JLOS_PDE_4MB_ADDR_MASK) | offset;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
    jlos_page_table_entry_t *pte = &page_table->entries[jlos_paging_get_page_table_index(va)];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    return ((*pte) & JLOS_PAGE_ADDR_MASK) | jlos_paging_get_page_offset(va);
}

void jlos_arch_change_flags_entry(jlos_paging_context_t *ctx, uint32_t va, uint32_t prot)
{
    if (!ctx || !ctx->root) {
        return;
    }
    uint32_t pd_idx = jlos_paging_get_page_dir_index(va);
    uint32_t pt_idx = jlos_paging_get_page_table_index(va);

    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[pd_idx];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return;
    }
    if (*pde & JLOS_PDE_4MB) {
        uint32_t phys_base = *pde & JLOS_PDE_4MB_ADDR_MASK;
        uint32_t keep = *pde & (JLOS_PDE_4MB | JLOS_PDE_GLOBAL | 0x00001000 |
            JLOS_PDE_WRITE_THROUGH | JLOS_PDE_CACHE_DISABLE | JLOS_PDE_ACCESSED | JLOS_PDE_DIRTY);
        *pde = phys_base | pte_prot(prot) | keep;
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

    *pte = phys | pte_prot(prot) | preserve;

    if (prot & JLOS_PG_USER) {
        *pde |= JLOS_PDE_USER;
    }
}

bool jlos_arch_need_flush_tlb(jlos_paging_context_t *ctx, uint32_t va)
{
    if (!ctx || !ctx->root) {
        return false;
    }
    jlos_paging_context_t *act = jlos_hal_paging_get_active_context();
    if (ctx == act) {
        return true;
    }
    if (!act || !act->root) {
        return false;
    }
    uint32_t pd_idx = jlos_paging_get_page_dir_index(va);
    jlos_page_dir_entry_t self_pde = ((jlos_page_dir_t *)ctx->root)->entries[pd_idx];
    if (!(self_pde & JLOS_PDE_PRESENT) || (self_pde & JLOS_PDE_4MB)) {
        return false;
    }
    jlos_page_dir_entry_t act_pde = ((jlos_page_dir_t *)act->root)->entries[pd_idx];
    if (!(act_pde & JLOS_PDE_PRESENT)) {
        return false;
    }
    if (self_pde & JLOS_PDE_4MB) {
        return (act_pde & JLOS_PDE_GLOBAL) != 0;
    }
    return (self_pde & JLOS_PAGE_ADDR_MASK) == (act_pde & JLOS_PAGE_ADDR_MASK);
}

void jlos_arch_paging_context_tables_destroy(jlos_paging_context_t *ctx)
{
    if (!ctx || !ctx->root) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&ctx->lock);
    for (uint32_t i = 0; i < JLOS_PAGE_DIR_ENTRIES; i++) {
        jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[i];
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
        if (is_kernel_space) {
            continue;
        }
        jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT((*pde) & JLOS_PAGE_ADDR_MASK);
        for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
            jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
            if (*pte & JLOS_PTE_PRESENT) {
                jlos_page_frame_refcount_dec(*pte & JLOS_PAGE_ADDR_MASK);
            }
        }
        jlos_page_frame_clear_owner_type(VIRT_TO_PHYS(pt));
        jlos_page_frame_free(pt);
    }
    jlos_spin_unlock_irqrestore(&ctx->lock, flags);
    jlos_page_frame_free(ctx->root);
    jlos_paging_context_init(ctx);
}

void jlos_arch_paging_context_tables_clone(jlos_paging_context_t *dst, jlos_paging_context_t *src)
{
    jlos_paging_context_init(dst);
    dst->root = jlos_page_frame_malloc();
    if (!dst->root) {
        return;
    }
    jlos_memset(dst->root, 0, sizeof(jlos_page_dir_t));
    if (!src->root) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&src->lock);
    dst->num_page_tables = 0;
    uint32_t kernel_pde_start = KERNEL_VIRTUAL_BASE >> 22;
    for (uint32_t i = kernel_pde_start; i < JLOS_PAGE_DIR_ENTRIES; i++) {
        jlos_page_dir_entry_t src_pde = ((jlos_page_dir_t *)src->root)->entries[i];
        if (!(src_pde & JLOS_PDE_PRESENT)) {
            continue;
        }
        ((jlos_page_dir_t *)dst->root)->entries[i] = src_pde;
    }
    jlos_spin_unlock_irqrestore(&src->lock, flags);
}

void jlos_arch_paging_initialize_kernel(jlos_paging_table_alloc_fn alloc_fn)
{
    s_page_table_alloc = alloc_fn;
    jlos_paging_context_init(&s_kernel_paging_context);

    /* 高半核：0xC0000000+ → PA, 覆盖全部物理内存 (限制在 1GB 内核空间内) */
    uint32_t map_size = jlos_device_physical_memory_end;
    if (map_size > KERNEL_DIRECT_MAP_SIZE) {
        map_size = KERNEL_DIRECT_MAP_SIZE;
    }
    uint32_t low_end = (uint32_t)&_boot_end_phys;
    uint32_t huge_start = JLOS_EXCEPT_CEIL(low_end, JLOS_PDE_4MB_SIZE);
    if (huge_start > map_size) {
        huge_start = map_size;
    }
    if (huge_start) {
        jlos_paging_map_range(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE, 0, huge_start,
            JLOS_PG_KERNEL_RW | JLOS_PG_GLOBAL);
    }
    /* .text 只读 */
    const jlos_hal_kernel_segments_t *segments = jlos_hal_get_kernel_segments();
    if (segments->text_start) {
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->text_start, segments->text_end,
            JLOS_PG_KERNEL_RO);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->data_start, segments->data_end,
            JLOS_PG_KERNEL_RW);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->bss_start, segments->bss_end,
            JLOS_PG_KERNEL_RW);
    }
    for (uint32_t pa = huge_start; pa < map_size; pa += JLOS_PDE_4MB_SIZE) {
        if (!root_ensure(&s_kernel_paging_context)) {
            break;
        }
        map_4mb_locked(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE + pa, pa,
            JLOS_PDE_GLOBAL);
    }
    jlos_paging_enable(&s_kernel_paging_context);
    jlos_hal_paging_enable_global_pages();
    s_page_table_alloc = NULL;
}

void jlos_arch_paging_print_states(jlos_paging_context_t *ctx)
{
    if (!ctx || !ctx->root) {
        printk_info("page context not initialized\n");
        return;
    }
    uint32_t mapped_pages = 0;
    uint32_t fl = jlos_spin_lock_irqsave(&ctx->lock);
    for (uint32_t pd_index = 0; pd_index < JLOS_PAGE_DIR_ENTRIES; pd_index++) {
        jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[pd_index];
        if (*pde & JLOS_PDE_PRESENT) {
            if (*pde & JLOS_PDE_4MB) {
                mapped_pages += JLOS_PAGE_TABLE_ENTRIES;
            } else {
                uint32_t pt_phys = *pde & JLOS_PAGE_ADDR_MASK;
                mapped_pages += jlos_page_frame_pt_present_count_get(pt_phys);
            }
        }
    }
    jlos_spin_unlock_irqrestore(&ctx->lock, fl);
    printk_info("page states:\n");
    printk_info("- page directory: 0x%x\n", ctx->root);
    printk_info("- page tables: %u\n", ctx->num_page_tables);
    printk_info("- mapped pages: %u (%u KB)\n", mapped_pages, mapped_pages * 4);
    printk_info("- total physical memory: %u KB\n", jlos_page_frame_get_total() * 4);
    printk_info("- free physical memory: %u KB\n", jlos_page_frame_get_free() * 4);
}

void jlos_arch_paging_free_user_pages(jlos_paging_context_t *ctx)
{
    if (!ctx || !ctx->root) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&ctx->lock);
    for (uint32_t pd = 0; pd < JLOS_PAGE_DIR_ENTRIES; pd++) {
        jlos_page_dir_entry_t pde = ((jlos_page_dir_t *)ctx->root)->entries[pd];
        if (!(pde & JLOS_PDE_PRESENT)) {
            continue;
        }
        if (pde & JLOS_PDE_4MB) {
            continue;
        }
        jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(pde & JLOS_PAGE_ADDR_MASK);
        for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
            jlos_page_table_entry_t pte = pt->entries[pt_idx];
            if (!((pte & JLOS_PTE_PRESENT))) {
                continue;
            }
            if (!(pte & JLOS_PTE_USER)) {
                continue;
            }
            jlos_page_frame_refcount_dec(pte & JLOS_PAGE_ADDR_MASK);
        }
    }
    jlos_spin_unlock_irqrestore(&ctx->lock, fl);
}
