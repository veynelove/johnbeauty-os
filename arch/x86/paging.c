/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/paging.h>
#include <hal/hal.h>
#include <hal/mmu.h>
#include <hal/smp.h>
#include <arch/x86/paging.h>
#include <arch/x86/smp.h>
#include <arch/x86/lapic.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/device.h>

#define JLOS_KERNEL_LOG_SUBSYS "paging"
#include <kernel/printk.h>

extern uint32_t                     _boot_end_phys;
extern uint32_t                     _kernel_end_phys;

#ifdef CONFIG_X86_64
extern uint64_t                     boot_pml4[];
#else
extern uint32_t                     boot_page_dir[];
#endif

jlos_paging_context_t               s_kernel_paging_context;
static jlos_paging_table_alloc_fn   s_page_table_alloc = nullptr;
static struct jlos_paging_context   *s_active_paging_context[JLOS_MAX_CPUS];

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
    if (prot & JLOS_PG_NOCACHE) {
        p |= JLOS_PTE_CACHE_DISABLE;
    }
    return p;
}

static jlos_page_table_entry_t *pte_walk(jlos_paging_context_t *ctx, uintptr_t va);

#ifdef CONFIG_X86_64
/* ============ 64 位：四级页表（PML4→PDPT→PD→PT，每级 512 项，PD 层 2MB 大页） ============ */

static void *paging_table_alloc(void)
{
    return s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
}

static jlos_page_table_entry_t *pte_walk(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (!ctx || !ctx->root) {
        return nullptr;
    }
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    jlos_page_dir_entry_t *pml4e = &pml4->entries[jlos_paging_get_pml4_index(va)];
    if (!(*pml4e & JLOS_PDE_PRESENT)) {
        return nullptr;
    }
    jlos_page_pdpt_t *pdpt = (jlos_page_pdpt_t *)PHYS_TO_VIRT(*pml4e & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pdpte = &pdpt->entries[jlos_paging_get_pdpt_index(va)];
    if (!(*pdpte & JLOS_PDE_PRESENT) || (*pdpte & JLOS_PDE_4MB)) {
        return nullptr;
    }
    jlos_page_dir_t *pd = (jlos_page_dir_t *)PHYS_TO_VIRT(*pdpte & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pde = &pd->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT) || (*pde & JLOS_PDE_4MB)) {
        return nullptr;
    }
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
    return &pt->entries[jlos_paging_get_page_table_index(va)];
}

static bool root_ensure(jlos_paging_context_t *ctx)
{
    if (ctx->root) {
        return true;
    }
    ctx->root = paging_table_alloc();
    if (!ctx->root) {
        return false;
    }
    jlos_memset(ctx->root, 0, sizeof(jlos_page_pml4_t));
    return true;
}

/* 中间层（PDPT/PD）逐级确保存在，返回该层虚拟地址指针；user 为 true 时层级带 User bit
 * （x86-64 四级页权限检查要求用户态访问时 PML4/PDPT/PD/PT 每层都置 User） */
static jlos_page_dir_entry_t *paging_level_ensure(jlos_paging_context_t *ctx, jlos_page_dir_entry_t *parent_entry,
    size_t table_size, bool user)
{
    if (*parent_entry & JLOS_PDE_PRESENT) {
        return (jlos_page_dir_entry_t *)PHYS_TO_VIRT(*parent_entry & JLOS_PAGING_ADDR_MASK);
    }
    void *table = paging_table_alloc();
    if (!table) {
        return nullptr;
    }
    jlos_memset(table, 0, table_size);
    *parent_entry = VIRT_TO_PHYS(table) | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (user ? JLOS_PDE_USER : 0);
    ctx->num_page_tables++;
    return (jlos_page_dir_entry_t *)PHYS_TO_VIRT(VIRT_TO_PHYS(table));
}



/* PD 层 2MB 大页映射（PML4[511]/PML4[256] 区间的大块映射用，内核映射不带 User） */
static bool map_large_locked(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t flags)
{
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    jlos_page_dir_entry_t *pml4e = &pml4->entries[jlos_paging_get_pml4_index(va)];
    jlos_page_dir_entry_t *pdpte_level = paging_level_ensure(ctx, pml4e, sizeof(jlos_page_pdpt_t), false);
    if (!pdpte_level) {
        return false;
    }
    jlos_page_dir_entry_t *pde_level = paging_level_ensure(ctx, &pdpte_level[jlos_paging_get_pdpt_index(va)],
        sizeof(jlos_page_dir_t), false);
    if (!pde_level) {
        return false;
    }
    jlos_page_dir_entry_t *pde = &pde_level[jlos_paging_get_page_dir_index(va)];
    if (*pde & JLOS_PDE_PRESENT) {
        return false;
    }
    *pde = (pa & JLOS_PAGING_LARGE_ADDR_MASK) | flags | JLOS_PDE_4MB | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE;
    return true;
}


bool jlos_arch_map_entry(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    if (!ctx) {
        return false;
    }
    if (!root_ensure(ctx)) {
        return false;
    }
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    jlos_page_dir_entry_t *pml4e = &pml4->entries[jlos_paging_get_pml4_index(va)];
    bool user = (pte_prot(prot) & JLOS_PTE_USER) != 0;
    jlos_page_dir_entry_t *pdpte_level = paging_level_ensure(ctx, pml4e, sizeof(jlos_page_pdpt_t), user);
    if (!pdpte_level) {
        printk_err("alloc pdpt failed\n");
        return false;
    }
    jlos_page_dir_entry_t *pde_level = paging_level_ensure(ctx, &pdpte_level[jlos_paging_get_pdpt_index(va)],
        sizeof(jlos_page_dir_t), user);
    if (!pde_level) {
        printk_err("alloc pd failed\n");
        return false;
    }
    jlos_page_dir_entry_t *pde = &pde_level[jlos_paging_get_page_dir_index(va)];
    jlos_page_table_t *page_table;
    uintptr_t pt_phys;
    if (*pde & JLOS_PDE_PRESENT) {
        pt_phys = *pde & JLOS_PAGING_ADDR_MASK;
        page_table = (jlos_page_table_t *)PHYS_TO_VIRT(pt_phys);
    } else {
        page_table = paging_table_alloc();
        if (!page_table) {
            printk_err("alloc page_table failed\n");
            return false;
        }
        jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
        pt_phys = VIRT_TO_PHYS(page_table);
        *pde = pt_phys | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (pte_prot(prot) & JLOS_PDE_USER);
        ctx->num_page_tables++;
        jlos_page_frame_set_owner_type((uint32_t)pt_phys, nullptr, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
        jlos_page_frame_pt_present_count_set((uint32_t)pt_phys, 0);
    }
    jlos_page_table_entry_t *pte = &page_table->entries[jlos_paging_get_page_table_index(va)];
    if (*pte & JLOS_PTE_PRESENT) {
        printk_err("remap present pte. va = 0x%lx, old = 0x%lx\n", va, *pte);
        return false;
    }
    *pte = (pa & JLOS_PAGING_ADDR_MASK) | pte_prot(prot);
    jlos_page_frame_pt_present_count_inc((uint32_t)pt_phys);
    return true;
}

static bool unmap_locked(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (!ctx || !ctx->root) {
        return false;
    }
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    jlos_page_dir_entry_t *pml4e = &pml4->entries[jlos_paging_get_pml4_index(va)];
    if (!(*pml4e & JLOS_PDE_PRESENT)) {
        return false;
    }
    jlos_page_pdpt_t *pdpt = (jlos_page_pdpt_t *)PHYS_TO_VIRT(*pml4e & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pdpte = &pdpt->entries[jlos_paging_get_pdpt_index(va)];
    if (!(*pdpte & JLOS_PDE_PRESENT) || (*pdpte & JLOS_PDE_4MB)) {
        return false;
    }
    jlos_page_dir_t *pd = (jlos_page_dir_t *)PHYS_TO_VIRT(*pdpte & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pde = &pd->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return false;
    }
    if (*pde & JLOS_PDE_4MB) {
        if (va < USER_SPACE_LIMIT) {
            uintptr_t phys_base = *pde & JLOS_PAGING_LARGE_ADDR_MASK;
            jlos_page_frame_free_n((void *)PHYS_TO_VIRT(phys_base), JLOS_PAGING_LARGE_SIZE / JLOS_PAGE_SIZE);
        }
        *pde = 0;
        return true;
    }
    uintptr_t pt_phys = *pde & JLOS_PAGING_ADDR_MASK;
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(pt_phys);
    jlos_page_table_entry_t *pte = &pt->entries[jlos_paging_get_page_table_index(va)];
    if (*pte & JLOS_PTE_PRESENT) {
        jlos_page_frame_refcount_dec((uint32_t)(*pte & JLOS_PAGING_ADDR_MASK));
        jlos_page_frame_pt_present_count_dec((uint32_t)pt_phys);
    }
    *pte = 0;
    if (jlos_page_frame_pt_present_count_get((uint32_t)pt_phys) == 0) {
        jlos_page_frame_clear_owner_type((uint32_t)pt_phys);
        if (va < USER_SPACE_LIMIT) {
            jlos_page_frame_free(pt);
        }
        *pde = 0;
        ctx->num_page_tables--;
    }
    return true;
}

bool jlos_arch_unmap_entry(jlos_paging_context_t *ctx, uintptr_t va)
{
    return unmap_locked(ctx, va);
}

bool jlos_arch_map_range_entry(jlos_paging_context_t *ctx, uintptr_t va_start, uintptr_t pa_start,
    size_t size, uint32_t prot)
{
    if (!ctx || !root_ensure(ctx)) {
        return false;
    }
    uintptr_t virtual_addr = va_start;
    uintptr_t physical_addr = pa_start;
    if (size && (va_start + size < va_start || pa_start + size < pa_start)) {
        return false;
    }
    uintptr_t end_addr = va_start + size;
    uint32_t pages_done = 0;

    while (virtual_addr < end_addr) {
        if (!jlos_arch_map_entry(ctx, virtual_addr, physical_addr, prot)) {
            goto rollback;
        }
        virtual_addr += JLOS_PAGE_SIZE;
        physical_addr += JLOS_PAGE_SIZE;
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

uintptr_t jlos_arch_get_phys_entry(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (!ctx || !ctx->root) {
        return va;
    }
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    jlos_page_dir_entry_t *pml4e = &pml4->entries[jlos_paging_get_pml4_index(va)];
    if (!(*pml4e & JLOS_PDE_PRESENT)) {
        return 0;
    }
    jlos_page_pdpt_t *pdpt = (jlos_page_pdpt_t *)PHYS_TO_VIRT(*pml4e & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pdpte = &pdpt->entries[jlos_paging_get_pdpt_index(va)];
    if (!(*pdpte & JLOS_PDE_PRESENT)) {
        return 0;
    }
    jlos_page_dir_t *pd = (jlos_page_dir_t *)PHYS_TO_VIRT(*pdpte & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pde = &pd->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return 0;
    }
    if (*pde & JLOS_PDE_4MB) {
        uintptr_t offset = va & (JLOS_PAGING_LARGE_SIZE - 1);
        return (*pde & JLOS_PAGING_LARGE_ADDR_MASK) | offset;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
    jlos_page_table_entry_t *pte = &page_table->entries[jlos_paging_get_page_table_index(va)];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    return (*pte & JLOS_PAGING_ADDR_MASK) | jlos_paging_get_page_offset(va);
}

void jlos_arch_change_flags_entry(jlos_paging_context_t *ctx, uintptr_t va, uint32_t prot)
{
    if (!ctx || !ctx->root) {
        return;
    }
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    jlos_page_dir_entry_t *pml4e = &pml4->entries[jlos_paging_get_pml4_index(va)];
    if (!(*pml4e & JLOS_PDE_PRESENT)) {
        return;
    }
    jlos_page_pdpt_t *pdpt = (jlos_page_pdpt_t *)PHYS_TO_VIRT(*pml4e & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pdpte = &pdpt->entries[jlos_paging_get_pdpt_index(va)];
    if (!(*pdpte & JLOS_PDE_PRESENT) || (*pdpte & JLOS_PDE_4MB)) {
        return;
    }
    jlos_page_dir_t *pd = (jlos_page_dir_t *)PHYS_TO_VIRT(*pdpte & JLOS_PAGING_ADDR_MASK);
    jlos_page_dir_entry_t *pde = &pd->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return;
    }
    if (prot & JLOS_PG_USER) {
        *pml4e |= JLOS_PDE_USER;
        *pdpte |= JLOS_PDE_USER;
    }
    if (*pde & JLOS_PDE_4MB) {
        uintptr_t phys_base = *pde & JLOS_PAGING_LARGE_ADDR_MASK;
        uintptr_t keep = *pde & ((uintptr_t)JLOS_PDE_4MB | JLOS_PDE_GLOBAL | 0x1000 |
            JLOS_PDE_WRITE_THROUGH | JLOS_PDE_CACHE_DISABLE | JLOS_PDE_ACCESSED | JLOS_PDE_DIRTY);
        *pde = phys_base | pte_prot(prot) | keep;
        return;
    }
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
    jlos_page_table_entry_t *pte = &pt->entries[jlos_paging_get_page_table_index(va)];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return;
    }
    uintptr_t phys = *pte & JLOS_PAGING_ADDR_MASK;
    uintptr_t preserve = *pte & ((uintptr_t)JLOS_PTE_GLOBAL |
        JLOS_PTE_DIRTY | JLOS_PTE_CACHE_DISABLE | JLOS_PTE_WRITE_THROUGH | JLOS_PTE_ACCESSED);

    *pte = phys | pte_prot(prot) | preserve;
}

bool jlos_arch_need_flush_tlb(jlos_paging_context_t *ctx, uintptr_t va)
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
    jlos_page_dir_entry_t self_pml4e = ((jlos_page_pml4_t *)ctx->root)->entries[jlos_paging_get_pml4_index(va)];
    if (!(self_pml4e & JLOS_PDE_PRESENT)) {
        return false;
    }
    jlos_page_dir_entry_t act_pml4e = ((jlos_page_pml4_t *)act->root)->entries[jlos_paging_get_pml4_index(va)];
    if (!(act_pml4e & JLOS_PDE_PRESENT)) {
        return false;
    }

    return (self_pml4e & JLOS_PAGING_ADDR_MASK) == (act_pml4e & JLOS_PAGING_ADDR_MASK);
}

void jlos_arch_paging_context_tables_destroy(jlos_paging_context_t *ctx)
{
    if (!ctx || !ctx->root) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&ctx->lock);
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    uint32_t user_pml4_end = (uint32_t)(USER_SPACE_LIMIT >> 39);
    for (uint32_t i = 0; i < user_pml4_end; i++) {
        jlos_page_dir_entry_t *pml4e = &pml4->entries[i];
        if (!(*pml4e & JLOS_PDE_PRESENT)) {
            continue;
        }
        jlos_page_pdpt_t *pdpt = (jlos_page_pdpt_t *)PHYS_TO_VIRT(*pml4e & JLOS_PAGING_ADDR_MASK);
        for (uint32_t j = 0; j < JLOS_PAGE_PDPT_ENTRIES; j++) {
            jlos_page_dir_entry_t *pdpte = &pdpt->entries[j];
            if (!(*pdpte & JLOS_PDE_PRESENT)) {
                continue;
            }
            if (*pdpte & JLOS_PDE_4MB) {
                continue;
            }
            jlos_page_dir_t *pd = (jlos_page_dir_t *)PHYS_TO_VIRT(*pdpte & JLOS_PAGING_ADDR_MASK);
            for (uint32_t k = 0; k < JLOS_PAGE_DIR_ENTRIES; k++) {
                jlos_page_dir_entry_t *pde = &pd->entries[k];
                if (!(*pde & JLOS_PDE_PRESENT)) {
                    continue;
                }
                if (*pde & JLOS_PDE_4MB) {
                    uintptr_t phys_base = *pde & JLOS_PAGING_LARGE_ADDR_MASK;
                    jlos_page_frame_free_order((void *)PHYS_TO_VIRT(phys_base), JLOS_PFA_MAX_ORDER);
                    continue;
                }
                jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
                for (uint32_t m = 0; m < JLOS_PAGE_TABLE_ENTRIES; m++) {
                    jlos_page_table_entry_t *pte = &pt->entries[m];
                    if (*pte & JLOS_PTE_PRESENT) {
                        jlos_page_frame_refcount_dec((uint32_t)(*pte & JLOS_PAGING_ADDR_MASK));
                    }
                }
                jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(pt));
                jlos_page_frame_free(pt);
            }
            jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(pd));
            jlos_page_frame_free(pd);
        }
        jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(pdpt));
        jlos_page_frame_free(pdpt);
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
    jlos_memset(dst->root, 0, sizeof(jlos_page_pml4_t));
    if (!src->root) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&src->lock);
    dst->num_page_tables = 0;
    uint32_t kernel_pml4_start = jlos_paging_get_pml4_index(KERNEL_DIRECT_MAP_BASE);
    jlos_page_pml4_t *src_pml4 = (jlos_page_pml4_t *)src->root;
    jlos_page_pml4_t *dst_pml4 = (jlos_page_pml4_t *)dst->root;
    for (uint32_t i = kernel_pml4_start; i < JLOS_PAGE_PML4_ENTRIES; i++) {
        jlos_page_dir_entry_t src_pml4e = src_pml4->entries[i];
        if (!(src_pml4e & JLOS_PDE_PRESENT)) {
            continue;
        }
        dst_pml4->entries[i] = src_pml4e;
    }
    jlos_spin_unlock_irqrestore(&src->lock, flags);
}

void jlos_arch_paging_initialize_kernel(jlos_paging_table_alloc_fn alloc_fn)
{
    s_page_table_alloc = alloc_fn;
    jlos_paging_context_init(&s_kernel_paging_context);

    uintptr_t map_size = (uintptr_t)jlos_device_physical_memory_end;
    if (map_size > KERNEL_DIRECT_MAP_SIZE) {
        map_size = KERNEL_DIRECT_MAP_SIZE;
    }
    uintptr_t low_end = (uintptr_t)&_boot_end_phys;
    uintptr_t huge_start = JLOS_ALIGN_UP(low_end, JLOS_PAGING_LARGE_SIZE);
    if (huge_start > map_size) {
        huge_start = map_size;
    }

    /* 内核镜像 4KB 映射（PML4[511] 区, text RO 粒度依赖） */
    uintptr_t kernel_end = (uintptr_t)&_kernel_end_phys;
    if (kernel_end) {
        jlos_paging_map_range(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE, 0, kernel_end,
            JLOS_PG_KERNEL_RW | JLOS_PG_GLOBAL);
    }

    /* direct map 低段 4KB 映射（[0, huge_start)） */
    if (huge_start) {
        jlos_paging_map_range(&s_kernel_paging_context, KERNEL_DIRECT_MAP_BASE, 0, huge_start,
            JLOS_PG_KERNEL_RW | JLOS_PG_GLOBAL);
    }

    const jlos_hal_kernel_segments_t *segments = jlos_hal_get_kernel_segments();
    if (segments->text_start) {
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->text_start, segments->text_end,
            JLOS_PG_KERNEL_RO);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->data_start, segments->data_end,
            JLOS_PG_KERNEL_RW);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->bss_start, segments->bss_end,
            JLOS_PG_KERNEL_RW);
    }

    /* direct map 剩余物理内存 2MB 大页 */
    for (uintptr_t pa = huge_start; pa < map_size; pa += JLOS_PAGING_LARGE_SIZE) {
        if (!root_ensure(&s_kernel_paging_context)) {
            break;
        }
        map_large_locked(&s_kernel_paging_context, KERNEL_DIRECT_MAP_BASE + pa, pa,
            JLOS_PDE_GLOBAL);
    }
    jlos_paging_enable(&s_kernel_paging_context);
    jlos_hal_paging_enable_global_pages();
    s_page_table_alloc = nullptr;
}

void jlos_arch_paging_print_states(jlos_paging_context_t *ctx)
{
    if (!ctx || !ctx->root) {
        printk_info("page context not initialized\n");
        return;
    }
    uint32_t mapped_pages = 0;
    uint32_t fl = jlos_spin_lock_irqsave(&ctx->lock);
    jlos_page_pml4_t *pml4 = (jlos_page_pml4_t *)ctx->root;
    for (uint32_t i = 0; i < JLOS_PAGE_PML4_ENTRIES; i++) {
        jlos_page_dir_entry_t *pml4e = &pml4->entries[i];
        if (!(*pml4e & JLOS_PDE_PRESENT)) {
            continue;
        }
        jlos_page_pdpt_t *pdpt = (jlos_page_pdpt_t *)PHYS_TO_VIRT(*pml4e & JLOS_PAGING_ADDR_MASK);
        for (uint32_t j = 0; j < JLOS_PAGE_PDPT_ENTRIES; j++) {
            jlos_page_dir_entry_t *pdpte = &pdpt->entries[j];
            if (!(*pdpte & JLOS_PDE_PRESENT)) {
                continue;
            }
            if (*pdpte & JLOS_PDE_4MB) {
                continue;
            }
            jlos_page_dir_t *pd = (jlos_page_dir_t *)PHYS_TO_VIRT(*pdpte & JLOS_PAGING_ADDR_MASK);
            for (uint32_t k = 0; k < JLOS_PAGE_DIR_ENTRIES; k++) {
                jlos_page_dir_entry_t *pde = &pd->entries[k];
                if (!(*pde & JLOS_PDE_PRESENT)) {
                    continue;
                }
                if (*pde & JLOS_PDE_4MB) {
                    mapped_pages += JLOS_PAGING_LARGE_SIZE / JLOS_PAGE_SIZE;
                } else {
                    uint32_t pt_phys = (uint32_t)(*pde & JLOS_PAGING_ADDR_MASK);
                    mapped_pages += jlos_page_frame_pt_present_count_get(pt_phys);
                }
            }
        }
    }
    jlos_spin_unlock_irqrestore(&ctx->lock, fl);
    printk_info("page states:\n");
    printk_info("- page directory: 0x%lx\n", (unsigned long)ctx->root);
    printk_info("- page tables: %u\n", ctx->num_page_tables);
    printk_info("- mapped pages: %u (%u KB)\n", mapped_pages, mapped_pages * 4);
    printk_info("- total physical memory: %u KB\n", jlos_page_frame_get_total() * 4);
    printk_info("- free physical memory: %u KB\n", jlos_page_frame_get_free() * 4);
}

#else
/* ============ 32 位：两级页表（PD→PT，每级 1024 项，PD 层 4MB 大页） ============ */

static void *paging_table_alloc(void)
{
    return s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
}

static jlos_page_table_entry_t *pte_walk(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (!ctx || !ctx->root) {
        return nullptr;
    }
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT) || (*pde & JLOS_PDE_4MB)) {
        return nullptr;
    }
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
    return &pt->entries[jlos_paging_get_page_table_index(va)];
}

static bool root_ensure(jlos_paging_context_t *ctx)
{
    if (ctx->root) {
        return true;
    }
    ctx->root = paging_table_alloc();
    if (!ctx->root) {
        return false;
    }
    jlos_memset(ctx->root, 0, sizeof(jlos_page_dir_t));
    return true;
}


static bool map_4mb_locked(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t pte_flags)
{
    uint32_t pd_index = jlos_paging_get_page_dir_index(va);
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[pd_index];
    if ((*pde & JLOS_PDE_PRESENT) && !(*pde & JLOS_PDE_4MB)) {
        jlos_page_table_t *old_pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
        for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
            if (old_pt->entries[pt_idx] & JLOS_PTE_PRESENT) {
                jlos_page_frame_free((void *)PHYS_TO_VIRT(old_pt->entries[pt_idx] & JLOS_PAGING_ADDR_MASK));
            }
        }
        jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(old_pt));
        jlos_page_frame_free(old_pt);
        ctx->num_page_tables--;
    }
    *pde = (pa & JLOS_PAGING_LARGE_ADDR_MASK) | pte_flags | JLOS_PDE_4MB | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE;
    return true;
}

bool jlos_arch_map_entry(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    if (!ctx) {
        return false;
    }
    if (!root_ensure(ctx)) {
        return false;
    }
    uint32_t pt_index = jlos_paging_get_page_table_index(va);
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[jlos_paging_get_page_dir_index(va)];
    jlos_page_table_t *page_table = nullptr;
    uintptr_t pt_phys;
    if (*pde & JLOS_PDE_PRESENT) {
        page_table = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
        pt_phys = *pde & JLOS_PAGING_ADDR_MASK;
    } else {
        page_table = paging_table_alloc();
        if (!page_table) {
            printk_err("alloc page_table failed\n");
            return false;
        }
        jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
        pt_phys = VIRT_TO_PHYS(page_table);
        *pde = pt_phys | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (pte_prot(prot) & JLOS_PDE_USER);
        ctx->num_page_tables++;
        jlos_page_frame_set_owner_type((uint32_t)pt_phys, nullptr, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
        jlos_page_frame_pt_present_count_set((uint32_t)pt_phys, 0);
    }
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    if (*pte & JLOS_PTE_PRESENT) {
        printk_err("remap present pte. va = 0x%lx, old = 0x%lx\n", va, *pte);
        return false;
    }
    *pte = (pa & JLOS_PAGING_ADDR_MASK) | pte_prot(prot);
    jlos_page_frame_pt_present_count_inc((uint32_t)pt_phys);
    return true;
}

static bool unmap_locked(jlos_paging_context_t *ctx, uintptr_t va)
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
        uintptr_t region_base = va & JLOS_PAGING_LARGE_ADDR_MASK;
        if (region_base < KERNEL_VIRTUAL_BASE) {
            uintptr_t phys_base = *pde & JLOS_PAGING_LARGE_ADDR_MASK;
            jlos_page_frame_free_n((void *)PHYS_TO_VIRT(phys_base), JLOS_PAGING_LARGE_SIZE / JLOS_PAGE_SIZE);
        }
        *pde = 0;
        return true;
    }
    uintptr_t pt_phys = *pde & JLOS_PAGING_ADDR_MASK;
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(pt_phys);
    jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
    if (*pte & JLOS_PTE_PRESENT) {
        jlos_page_frame_refcount_dec((uint32_t)(*pte & JLOS_PAGING_ADDR_MASK));
        jlos_page_frame_pt_present_count_dec((uint32_t)pt_phys);
    }
    *pte = 0;
    if (jlos_page_frame_pt_present_count_get((uint32_t)pt_phys) == 0) {
        uintptr_t region_base = va & JLOS_PAGING_LARGE_ADDR_MASK;
        jlos_page_frame_clear_owner_type((uint32_t)pt_phys);
        if (region_base < KERNEL_VIRTUAL_BASE) {
            jlos_page_frame_free(pt);
        }
        *pde = 0;
        ctx->num_page_tables--;
    }
    return true;
}

bool jlos_arch_unmap_entry(jlos_paging_context_t *ctx, uintptr_t va)
{
    return unmap_locked(ctx, va);
}

bool jlos_arch_map_range_entry(jlos_paging_context_t *ctx, uintptr_t va_start, uintptr_t pa_start,
    size_t size, uint32_t prot)
{
    if (!ctx || !root_ensure(ctx)) {
        return false;
    }
    uintptr_t virtual_addr = va_start;
    uintptr_t physical_addr = pa_start;
    if (size && (va_start + size < va_start || pa_start + size < pa_start)) {
        return false;
    }
    uintptr_t end_addr = va_start + size;
    uint32_t pages_done = 0;

    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    while (virtual_addr < end_addr) {
        uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);
        jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[pd_index];
        jlos_page_table_t *page_table = nullptr;
        uintptr_t pt_phys;
        if (*pde & JLOS_PDE_PRESENT) {
            page_table = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
            pt_phys = *pde & JLOS_PAGING_ADDR_MASK;
        } else {
            page_table = paging_table_alloc();
            if (!page_table) {
                printk_err("alloc page_table failed\n");
                goto rollback;
            }
            jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
            pt_phys = VIRT_TO_PHYS(page_table);
            *pde = pt_phys | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (pte_prot(prot) & JLOS_PDE_USER);
            ctx->num_page_tables++;
            jlos_page_frame_set_owner_type((uint32_t)pt_phys, nullptr, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
            jlos_page_frame_pt_present_count_set((uint32_t)pt_phys, 0);
        }
        jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
        if (*pte & JLOS_PTE_PRESENT) {
            printk_err("remap present pte. va = 0x%lx, old = 0x%lx\n", virtual_addr, *pte);
            goto rollback;
        }
        *pte = (physical_addr & JLOS_PAGING_ADDR_MASK) | pte_prot(prot);
        jlos_page_frame_pt_present_count_inc((uint32_t)pt_phys);
        virtual_addr += JLOS_PAGE_SIZE;
        physical_addr += JLOS_PAGE_SIZE;
        if ((virtual_addr & (JLOS_PAGING_LARGE_SIZE - 1)) == 0) {
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

uintptr_t jlos_arch_get_phys_entry(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (!ctx || !ctx->root) {
        return va;
    }
    jlos_page_dir_entry_t *pde = &((jlos_page_dir_t *)ctx->root)->entries[jlos_paging_get_page_dir_index(va)];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return 0;
    }
    if (*pde & JLOS_PDE_4MB) {
        uintptr_t offset = va & (JLOS_PAGING_LARGE_SIZE - 1);
        return (*pde & JLOS_PAGING_LARGE_ADDR_MASK) | offset;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
    jlos_page_table_entry_t *pte = &page_table->entries[jlos_paging_get_page_table_index(va)];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    return (*pte & JLOS_PAGING_ADDR_MASK) | jlos_paging_get_page_offset(va);
}

void jlos_arch_change_flags_entry(jlos_paging_context_t *ctx, uintptr_t va, uint32_t prot)
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
        uintptr_t phys_base = *pde & JLOS_PAGING_LARGE_ADDR_MASK;
        uintptr_t keep = *pde & ((uintptr_t)JLOS_PDE_4MB | JLOS_PDE_GLOBAL | 0x1000 |
            JLOS_PDE_WRITE_THROUGH | JLOS_PDE_CACHE_DISABLE | JLOS_PDE_ACCESSED | JLOS_PDE_DIRTY);
        *pde = phys_base | pte_prot(prot) | keep;
        return;
    }
    jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
    jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return;
    }
    uintptr_t phys = *pte & JLOS_PAGING_ADDR_MASK;
    uintptr_t preserve = *pte & ((uintptr_t)JLOS_PTE_GLOBAL |
        JLOS_PTE_DIRTY | JLOS_PTE_CACHE_DISABLE | JLOS_PTE_WRITE_THROUGH | JLOS_PTE_ACCESSED);

    *pte = phys | pte_prot(prot) | preserve;

    if (prot & JLOS_PG_USER) {
        *pde |= JLOS_PDE_USER;
    }
}

bool jlos_arch_need_flush_tlb(jlos_paging_context_t *ctx, uintptr_t va)
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

    return (self_pde & JLOS_PAGING_ADDR_MASK) == (act_pde & JLOS_PAGING_ADDR_MASK);
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
        uintptr_t vir_addr = (uintptr_t)i << 22;
        bool is_kernel_space = (vir_addr >= KERNEL_VIRTUAL_BASE);

        if (*pde & JLOS_PDE_4MB) {
            if (!is_kernel_space) {
                uintptr_t phys_base = *pde & JLOS_PAGING_LARGE_ADDR_MASK;
                jlos_page_frame_free_order((void *)PHYS_TO_VIRT(phys_base), JLOS_PFA_MAX_ORDER);
            }
            continue;
        }
        if (is_kernel_space) {
            continue;
        }
        jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGING_ADDR_MASK);
        for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
            jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
            if (*pte & JLOS_PTE_PRESENT) {
                jlos_page_frame_refcount_dec((uint32_t)(*pte & JLOS_PAGING_ADDR_MASK));
            }
        }
        jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(pt));
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
    uint32_t kernel_pde_start = (uint32_t)(KERNEL_VIRTUAL_BASE >> 22);
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

    uintptr_t map_size = (uintptr_t)jlos_device_physical_memory_end;
    if (map_size > KERNEL_DIRECT_MAP_SIZE) {
        map_size = KERNEL_DIRECT_MAP_SIZE;
    }
    uintptr_t low_end = (uintptr_t)&_boot_end_phys;
    uintptr_t huge_start = JLOS_EXCEPT_CEIL(low_end, JLOS_PAGING_LARGE_SIZE);
    if (huge_start > map_size) {
        huge_start = map_size;
    }
    if (huge_start) {
        jlos_paging_map_range(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE, 0, huge_start,
            JLOS_PG_KERNEL_RW | JLOS_PG_GLOBAL);
    }

    const jlos_hal_kernel_segments_t *segments = jlos_hal_get_kernel_segments();
    if (segments->text_start) {
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->text_start, segments->text_end,
            JLOS_PG_KERNEL_RO);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->data_start, segments->data_end,
            JLOS_PG_KERNEL_RW);
        jlos_paging_change_flags_range(&s_kernel_paging_context, segments->bss_start, segments->bss_end,
            JLOS_PG_KERNEL_RW);
    }
    for (uintptr_t pa = huge_start; pa < map_size; pa += JLOS_PAGING_LARGE_SIZE) {
        if (!root_ensure(&s_kernel_paging_context)) {
            break;
        }
        map_4mb_locked(&s_kernel_paging_context, KERNEL_VIRTUAL_BASE + pa, pa,
            JLOS_PDE_GLOBAL);
    }
    jlos_paging_enable(&s_kernel_paging_context);
    jlos_hal_paging_enable_global_pages();
    s_page_table_alloc = nullptr;
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
                uint32_t pt_phys = (uint32_t)(*pde & JLOS_PAGING_ADDR_MASK);
                mapped_pages += jlos_page_frame_pt_present_count_get(pt_phys);
            }
        }
    }
    jlos_spin_unlock_irqrestore(&ctx->lock, fl);
    printk_info("page states:\n");
    printk_info("- page directory: 0x%lx\n", (unsigned long)ctx->root);
    printk_info("- page tables: %u\n", ctx->num_page_tables);
    printk_info("- mapped pages: %u (%u KB)\n", mapped_pages, mapped_pages * 4);
    printk_info("- total physical memory: %u KB\n", jlos_page_frame_get_total() * 4);
    printk_info("- free physical memory: %u KB\n", jlos_page_frame_get_free() * 4);
}

#endif

/* ============ PTE 底层操作（架构无关，只依赖 pte_walk） ============ */

bool jlos_arch_pte_present(jlos_paging_context_t *ctx, uintptr_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    return pte && (*pte & JLOS_PTE_PRESENT);
}

uintptr_t jlos_arch_pte_phys(jlos_paging_context_t *ctx, uintptr_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    return *pte & JLOS_PAGING_ADDR_MASK;
}

void jlos_arch_pte_set(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (pte) {
        *pte = (pa & JLOS_PAGING_ADDR_MASK) | pte_prot(prot);
    }
}

void jlos_arch_pte_set_rw(jlos_paging_context_t *ctx, uintptr_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (pte && (*pte & JLOS_PTE_PRESENT)) {
        *pte |= JLOS_PTE_WRITABLE;
    }
}

void jlos_arch_pte_make_ro(jlos_paging_context_t *ctx, uintptr_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (pte) {
        *pte &= ~JLOS_PTE_WRITABLE;
    }
}

bool jlos_arch_pte_replace(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_PTE_PRESENT)) {
        return false;
    }
    *pte = (pa & JLOS_PAGING_ADDR_MASK) | pte_prot(prot);
    return true;
}

uint32_t jlos_arch_pte_get_prot(jlos_paging_context_t *ctx, uintptr_t va)
{
    jlos_page_table_entry_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    uint32_t prot = JLOS_PG_READ;
    if (*pte & JLOS_PTE_WRITABLE) {
        prot |= JLOS_PG_WRITE;
    }
    if (*pte & JLOS_PTE_USER) {
        prot |= JLOS_PG_USER;
    }
    if (*pte & JLOS_PTE_GLOBAL) {
        prot |= JLOS_PG_GLOBAL;
    }
    return prot;
}

void jlos_arch_paging_free_boot_tables(void)
{
#ifdef CONFIG_X86_64
    jlos_page_frame_refcount_dec((uint32_t)(uintptr_t)boot_pml4);
#else
    jlos_page_frame_refcount_dec((uint32_t)boot_page_dir);
#endif
}

void jlos_hal_paging_tlb_shootdown(uint32_t cpu_mask, uintptr_t va)
{
    uint32_t this_cpu = jlos_hal_get_cpu_id();
    uint32_t num_cpus = jlos_hal_num_cpus();
    jlos_hal_paging_flush_tlb(va);
    for (uint32_t cpu = 0; cpu < num_cpus; cpu++) {
        if (cpu == this_cpu || !(cpu_mask & (1u << cpu))) {
            continue;
        }
        per_cpu_write_cpu(jlos_cpu_tlb_flush_va, cpu, va);
        per_cpu_write_cpu(jlos_cpu_tlb_flush_done, cpu, false);
        jlos_arch_smp_send_ipi(cpu, JLOS_ARCH_IPI_TLB_SHOOTDOWN_VECTOR);
    }
    for (uint32_t cpu = 0; cpu < num_cpus; cpu++) {
        if (cpu == this_cpu || !(cpu_mask & (1u << cpu))) {
            continue;
        }
        while (!per_cpu_read_cpu(jlos_cpu_tlb_flush_done, cpu)) {
            __asm__ __volatile__("pause" : : : "memory");
        }
    }
}

void jlos_hal_paging_tlb_shootdown_all(uintptr_t va)
{
    uint32_t this_cpu = jlos_hal_get_cpu_id();
    uint32_t num_cpus = jlos_hal_num_cpus();
    uint32_t mask = 0;
    for (uint32_t cpu = 0; cpu < num_cpus; cpu++) {
        if (cpu != this_cpu) {
            mask |= (1u << cpu);
        }
    }
    jlos_hal_paging_tlb_shootdown(mask, va);
}

void jlos_hal_paging_enable(uintptr_t root_physical_addr)
{
#ifdef CONFIG_X86_64
    __asm__ __volatile__(
        "movq %0, %%cr3\n\t"
        "movq %%cr0, %%rax\n\t"
        "orl $0x80010001, %%eax\n\t"
        "movq %%rax, %%cr0\n\t"
        : : "r"((uint64_t)root_physical_addr) : "rax"
    );
#else
    __asm__ __volatile__(
        "movl %0, %%cr3\n\t"
        "movl %%cr0, %%eax\n\t"
        "orl $0x80010001, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        : : "r"((uint32_t)root_physical_addr) : "eax"
    );
#endif
}

void jlos_hal_paging_disable(void)
{
#ifdef CONFIG_X86_64
    __asm__ __volatile__(
        "movq %%cr0, %%rax\n\t"
        "andl $~0x80000000, %%eax\n\t"
        "movq %%rax, %%cr0\n\t"
        : : : "rax"
    );
#else
    __asm__ __volatile__(
        "movl %%cr0, %%eax\n\t"
        "andl $~0x80000000, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        : : : "eax"
    );
#endif
}

void jlos_hal_paging_switch(uintptr_t root_physical_addr)
{
#ifdef CONFIG_X86_64
    __asm__ __volatile__(
        "movq %0, %%cr3\n\t"
        : : "r"((uint64_t)root_physical_addr) : "memory"
    );
#else
    __asm__ __volatile__(
        "movl %0, %%cr3\n\t"
        : : "r"((uint32_t)root_physical_addr) : "memory"
    );
#endif
}

void jlos_hal_paging_flush_tlb(uintptr_t virtual_addr)
{
    __asm__ __volatile__(
        "invlpg (%0)\n\t"
        : : "r"(virtual_addr) : "memory"
    );
}

void jlos_hal_paging_flush_all_tlb(void)
{
#ifdef CONFIG_X86_64
    uint64_t cr4;
    __asm__ __volatile__(
        "movq %%cr4, %0\n\t"
        "andq %1, %0\n\t"
        "movq %0, %%cr4\n\t"
        "orq %2, %0\n\t"
        "movq %0, %%cr4\n\t"
        : "=&r"(cr4)
        : "r"(~(uint64_t)0x80), "r"((uint64_t)0x80)
        : "memory"
    );
#else
    uint32_t cr4;
    __asm__ __volatile__(
        "movl %%cr4, %0\n\t"
        "andl %1, %0\n\t"
        "movl %0, %%cr4\n\t"
        "orl %2, %0\n\t"
        "movl %0, %%cr4\n\t"
        : "=&r"(cr4)
        : "r"(~0x80u), "r"(0x80u)
        : "memory"
    );
#endif
}

uintptr_t jlos_hal_paging_get_fault_addr(void)
{
#ifdef CONFIG_X86_64
    uint64_t cr2;
    __asm__ __volatile__(
        "movq %%cr2, %0\n\t"
        : "=r"(cr2)
    );
    return (uintptr_t)cr2;
#else
    uint32_t cr2;
    __asm__ __volatile__(
        "movl %%cr2, %0\n\t"
        : "=r"(cr2)
    );
    return (uintptr_t)cr2;
#endif
}

bool jlos_hal_paging_supports_large_pages(void)
{
#ifdef CONFIG_X86_64
    return true;
#else
    uint32_t eax, ebx, ecx, edx;

    __asm__ __volatile__(
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );

    return (edx & (1 << 3)) != 0;
#endif
}

struct jlos_paging_context *jlos_hal_paging_get_active_context(void)
{
    return s_active_paging_context[jlos_hal_get_cpu_id()];
}

void jlos_hal_paging_set_active_context(struct jlos_paging_context *ctx)
{
    s_active_paging_context[jlos_hal_get_cpu_id()] = ctx;
}

void jlos_hal_paging_enable_global_pages(void)
{
#ifdef CONFIG_X86_64
    uint64_t cr4;
    __asm__ __volatile__(
        "movq %%cr4, %0\n\t"
        "orq $0x80, %0\n\t"
        "movq %0, %%cr4\n\t"
        : "=r"(cr4) : : "memory"
    );
#else
    uint32_t cr4;
    __asm__ __volatile__(
        "movl %%cr4, %0\n\t"
        "orl $0x80, %0\n\t"
        "movl %0, %%cr4\n\t"
        : "=r"(cr4) : : "memory"
    );
#endif
}

uint32_t jlos_hal_paging_asid_alloc(void)
{
    return 0;
}

void jlos_hal_paging_asid_free(uint32_t asid)
{
    (void)asid;
}
