/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/paging.h>
#include <arch/arm64/smp.h>
#include <arch/arm64/interrupts.h>
#include <hal/hal.h>
#include <hal/mmu.h>
#include <hal/smp.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>

#define JLOS_KERNEL_LOG_SUBSYS "paging"
#include <kernel/printk.h>

extern char __pgtable_start[];

jlos_paging_context_t               s_kernel_paging_context;
static struct jlos_paging_context   *s_active_context;
static jlos_paging_table_alloc_fn   s_page_table_alloc = nullptr;
static uint64_t                     *s_kernel_l0;

static void *paging_table_alloc(void)
{
    return s_page_table_alloc ? s_page_table_alloc() : jlos_page_frame_malloc();
}

void jlos_hal_paging_flush_tlb(uintptr_t virtual_addr)
{
    __asm__ __volatile__(
        "dsb ishst\n\t"
        "tlbi vaae1is, %0\n\t"
        "dsb ish\n\t"
        "isb"
        : : "r"(virtual_addr >> JLOS_PAGE_SHIFT) : "memory"
    );
}

void jlos_hal_paging_flush_all_tlb(void)
{
    __asm__ __volatile__(
        "tlbi vmalle1is\n\t"
        "dsb ish\n\t"
        "isb"
        : : : "memory"
    );
}

uintptr_t jlos_hal_paging_get_fault_addr(void)
{
    uintptr_t far;
    __asm__ __volatile__("mrs %0, far_el1" : "=r"(far));
    return far;
}

/* 仅内核上下文使用（写 TTBR1，幂等）；用户上下文一律走 jlos_hal_paging_switch */
void jlos_hal_paging_enable(uintptr_t root_physical_addr)
{
    __asm__ __volatile__(
        "msr ttbr1_el1, %0\n\t"
        "isb"
        : : "r"((uint64_t)root_physical_addr) : "memory"
    );
}

void jlos_hal_paging_switch(uintptr_t root_physical_addr, uint32_t asid)
{
    uint64_t ttbr0 = (uint64_t)root_physical_addr | ((uint64_t)(asid & 0xFF) << JLOS_ARM64_ASID_SHIFT);
    __asm__ __volatile__(
        "msr ttbr0_el1, %0\n\t"
        "isb"
        : : "r"(ttbr0) : "memory"
    );
}

void jlos_hal_paging_disable(void)
{

}

void jlos_hal_paging_enable_global_pages(void)
{

}

bool jlos_hal_paging_supports_large_pages(void)
{
    return true;
}

static uint32_t s_next_asid = 1;
static jlos_spinlock_t s_asid_lock = JLOS_SPINLOCK_INIT;

uint32_t jlos_hal_paging_asid_alloc(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_asid_lock);
    uint32_t asid = s_next_asid++;
    if (s_next_asid >= JLOS_ARM64_MAX_ASID) {
        s_next_asid = 1;
    }
    __asm__ __volatile__(
        "dsb ishst\n\t"
        "tlbi aside1is, %0\n\t"
        "dsb ish\n\t"
        "isb"
        : : "r"((uint64_t)asid) : "memory"
    );
    jlos_spin_unlock_irqrestore(&s_asid_lock, flags);
    return asid;
}

void jlos_hal_paging_asid_free(uint32_t asid)
{
    (void)asid;
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
        *per_cpu_ptr_cpu(jlos_cpu_tlb_flush_va, cpu) = va;
        *per_cpu_ptr_cpu(jlos_cpu_tlb_flush_done, cpu) = 0;
        jlos_arch_smp_send_ipi(cpu, JLOS_ARCH_ARM64_IPI_TLB_SGI);
    }
    for (uint32_t cpu = 0; cpu < num_cpus; cpu++) {
        if (cpu == this_cpu || !(cpu_mask & (1u << cpu))) {
            continue;
        }
        while (*per_cpu_ptr_cpu(jlos_cpu_tlb_flush_done, cpu) == 0) {
            __asm__ __volatile__("yield" : : : "memory");
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

jlos_paging_context_t *jlos_hal_paging_get_active_context(void)
{
    return s_active_context;
}

void jlos_hal_paging_set_active_context(jlos_paging_context_t *ctx)
{
    s_active_context = ctx;
}

void jlos_mmu_init(void)
{

}

jlos_mmu_t *jlos_mmu_get_kernel(void)
{
    return nullptr;
}

static uint64_t pte_prot(uint32_t prot)
{
    uint64_t p = JLOS_ARM64_DESC_TABLE | JLOS_ARM64_PTE_AF | JLOS_ARM64_PTE_SH_IS | (JLOS_ARM64_ATTR_NORMAL_WB << JLOS_ARM64_PTE_ATTRINDX_SHIFT);
    if (!(prot & JLOS_PG_WRITE)) {
        p |= JLOS_ARM64_PTE_AP2_RO;
    }
    if (prot & JLOS_PG_USER) {
        p |= JLOS_ARM64_PTE_AP1_USER | JLOS_ARM64_PTE_NG;
    }
    if (!(prot & JLOS_PG_EXEC)) {
        p |= JLOS_ARM64_PTE_PXN | JLOS_ARM64_PTE_UXN;
    }
    if (prot & JLOS_PG_NOCACHE) {
        p = (p & ~JLOS_ARM64_PTE_ATTRINDX_MASK) | (JLOS_ARM64_ATTR_NORMAL_NC << JLOS_ARM64_PTE_ATTRINDX_SHIFT);
    }
    return p;
}

static uint64_t *table_root(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (va >= KERNEL_DIRECT_MAP_BASE) {
        return s_kernel_l0;
    }
    return ctx ? (uint64_t *)ctx->root : nullptr;
}

static bool root_ensure(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (va >= KERNEL_DIRECT_MAP_BASE) {
        return s_kernel_l0 != nullptr;
    }
    if (!ctx) {
        return false;
    }
    if (!ctx->root) {
        ctx->root = paging_table_alloc();
        if (!ctx->root) {
            return false;
        }
        jlos_memset(ctx->root, 0, JLOS_ARM64_TABLE_SIZE);
    }
    return true;
}

static uint64_t *table_of(uint64_t entry)
{
    return (uint64_t *)PHYS_TO_VIRT(entry & JLOS_ARM64_PTE_ADDR_MASK);
}

static bool entry_is_table(uint64_t entry)
{
    return (entry & 0x3UL) == JLOS_ARM64_DESC_TABLE;
}

static bool entry_is_block(uint64_t entry)
{
    return (entry & 0x3UL) == JLOS_ARM64_DESC_BLOCK;
}

static uint64_t *table_entry(uint64_t *parent, uint32_t index)
{
    return entry_is_table(parent[index]) ? table_of(parent[index]) : nullptr;
}

static uint64_t *level_table(jlos_paging_context_t *ctx, uintptr_t va, uint32_t level)
{
    uint64_t *table = table_root(ctx, va);
    for (uint32_t i = 0; i < level && table; i++) {
        table = table_entry(table, jlos_arm64_level_index(va, i));
    }
    return table;
}

static uint64_t *pte_walk(jlos_paging_context_t *ctx, uintptr_t va)
{
    uint64_t *l3 = level_table(ctx, va, JLOS_ARM64_PTE_LEVEL);
    return l3 ? &l3[jlos_arm64_level_index(va, JLOS_ARM64_PTE_LEVEL)] : nullptr;
}

static uint64_t *level_ensure(jlos_paging_context_t *ctx, uint64_t *parent, uint32_t index)
{
    if (entry_is_table(parent[index])) {
        return table_of(parent[index]);
    }
    if (entry_is_block(parent[index])) {
        return nullptr;
    }
    uint64_t *table = paging_table_alloc();
    if (!table) {
        return nullptr;
    }
    jlos_memset(table, 0, JLOS_ARM64_TABLE_SIZE);
    uintptr_t pt_phys = VIRT_TO_PHYS(table);
    parent[index] = pt_phys | JLOS_ARM64_DESC_TABLE;
    ctx->num_page_tables++;
    jlos_page_frame_set_owner_type((uint32_t)pt_phys, nullptr, JLOS_PAGE_FRAME_TYPE_PAGE_TABLE);
    jlos_page_frame_pt_present_count_set((uint32_t)pt_phys, 0);
    return table;
}

bool jlos_arch_map_entry(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    if (!root_ensure(ctx, va)) {
        return false;
    }
    uint64_t *table = table_root(ctx, va);
    if (!table) {
        return false;
    }
    for (uint32_t level = 0; level < JLOS_ARM64_PTE_LEVEL; level++) {
        table = level_ensure(ctx, table, jlos_arm64_level_index(va, level));
        if (!table) {
            printk_err("alloc level %u table failed. va = 0x%lx\n", level, va);
            return false;
        }
    }
    uint64_t *pte = &table[jlos_arm64_level_index(va, JLOS_ARM64_PTE_LEVEL)];
    if (*pte & JLOS_ARM64_DESC_VALID) {
        printk_err("remap present pte. va = 0x%lx, old = 0x%lx\n", va, *pte);
        return false;
    }
    *pte = (pa & JLOS_ARM64_PTE_ADDR_MASK) | pte_prot(prot);
    jlos_page_frame_pt_present_count_inc((uint32_t)VIRT_TO_PHYS(table));
    return true;
}

static bool unmap_locked(jlos_paging_context_t *ctx, uintptr_t va)
{
    uint64_t *l2 = level_table(ctx, va, JLOS_ARM64_PTE_LEVEL - 1);
    if (!l2) {
        return false;
    }
    uint64_t *l3 = table_entry(l2, jlos_arm64_level_index(va, JLOS_ARM64_PTE_LEVEL - 1));
    if (!l3) {
        return false;
    }
    uint64_t *pte = &l3[jlos_arm64_level_index(va, JLOS_ARM64_PTE_LEVEL)];
    if (!(*pte & JLOS_ARM64_DESC_VALID)) {
        return false;
    }
    uintptr_t pt_phys = (uintptr_t)VIRT_TO_PHYS(l3);
    jlos_page_frame_refcount_dec((uint32_t)(*pte & JLOS_ARM64_PTE_ADDR_MASK));
    jlos_page_frame_pt_present_count_dec((uint32_t)pt_phys);
    *pte = 0;
    if (jlos_page_frame_pt_present_count_get((uint32_t)pt_phys) == 0) {
        jlos_page_frame_clear_owner_type((uint32_t)pt_phys);
        if (va < USER_SPACE_LIMIT) {
            jlos_page_frame_free(l3);
        }
        l2[jlos_arm64_level_index(va, JLOS_ARM64_PTE_LEVEL - 1)] = 0;
        ctx->num_page_tables--;
    }
    return true;
}

bool jlos_arch_unmap_entry(jlos_paging_context_t *ctx, uintptr_t va)
{
    return unmap_locked(ctx, va);
}

static bool jlos_arm64_map_block_2mb(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    uint64_t *table = table_root(ctx, va);
    if (!table) {
        return false;
    }
    for (uint32_t level = 0; level < JLOS_ARM64_PTE_LEVEL - 1; level++) {
        table = level_ensure(ctx, table, jlos_arm64_level_index(va, level));
        if (!table) {
            return false;
        }
    }
    uint64_t *pmd = &table[jlos_arm64_level_index(va, JLOS_ARM64_PTE_LEVEL - 1)];
    if (*pmd & JLOS_ARM64_DESC_VALID) {
        return false;
    }
    *pmd = (pa & JLOS_ARM64_PMD_ADDR_MASK) | (pte_prot(prot) & ~JLOS_ARM64_DESC_TABLE);
    return true;
}

bool jlos_arch_map_range_entry(jlos_paging_context_t *ctx, uintptr_t va_start, uintptr_t pa_start, size_t size, uint32_t prot)
{
    if (!ctx) {
        return false;
    }
    if (size && (va_start + size < va_start || pa_start + size < pa_start)) {
        return false;
    }
    uintptr_t vir_addr = va_start;
    uintptr_t phys_addr = pa_start;
    uintptr_t end_addr = va_start + size;
    uint32_t pages_done = 0;
    if (vir_addr >= KERNEL_DIRECT_MAP_BASE) {
        while ((vir_addr & (JLOS_ARM64_PMD_BLOCK_SIZE - 1)) == 0 &&
               (phys_addr & (JLOS_ARM64_PMD_BLOCK_SIZE - 1)) == 0 &&
               vir_addr + JLOS_ARM64_PMD_BLOCK_SIZE <= end_addr) {
            if (!jlos_arm64_map_block_2mb(ctx, vir_addr, phys_addr, prot)) {
                break;
            }
            vir_addr += JLOS_ARM64_PMD_BLOCK_SIZE;
            phys_addr += JLOS_ARM64_PMD_BLOCK_SIZE;
        }
    }
    while (vir_addr < end_addr) {
        if (!jlos_arch_map_entry(ctx, vir_addr, phys_addr, prot)) {
            goto rollback;
        }
        vir_addr += JLOS_PAGE_SIZE;
        phys_addr += JLOS_PAGE_SIZE;
        pages_done++;
    }
    return true;

rollback:
    for (uint32_t i = 0; i < pages_done; i++) {
        vir_addr -= JLOS_PAGE_SIZE;
        unmap_locked(ctx, vir_addr);
    }
    return false;
}

uintptr_t jlos_arch_get_phys_entry(jlos_paging_context_t *ctx, uintptr_t va)
{
    if (!ctx) {
        return va;
    }
    uint64_t *pte = pte_walk(ctx, va);
    if (pte && (*pte & JLOS_ARM64_DESC_VALID)) {
        return (*pte & JLOS_ARM64_PTE_ADDR_MASK) | (va & (JLOS_PAGE_SIZE - 1));
    }
    uint64_t *l1 = level_table(ctx, va, 1);
    if (l1 && entry_is_block(l1[jlos_arm64_level_index(va, 1)])) {
        uint64_t e = l1[jlos_arm64_level_index(va, 1)];
        return (e & JLOS_ARM64_PUD_ADDR_MASK) | (va & (JLOS_ARM64_PUD_BLOCK_SIZE - 1));
    }
    uint64_t *l2 = level_table(ctx, va, 2);
    if (l2 && entry_is_block(l2[jlos_arm64_level_index(va, 2)])) {
        uint64_t e = l2[jlos_arm64_level_index(va, 2)];
        return (e & JLOS_ARM64_PMD_ADDR_MASK) | (va & (JLOS_ARM64_PMD_BLOCK_SIZE - 1));
    }
    return 0;
}

void jlos_arch_change_flags_entry(jlos_paging_context_t *ctx, uintptr_t va, uint32_t prot)
{
    uint64_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_ARM64_DESC_VALID)) {
        return;
    }
    *pte = (*pte & JLOS_ARM64_PTE_ADDR_MASK) | pte_prot(prot);
}

bool jlos_arch_need_flush_tlb(jlos_paging_context_t *ctx, uintptr_t va)
{
    (void)va;
    return ctx == jlos_hal_paging_get_active_context() || ctx == &s_kernel_paging_context;
}

bool jlos_arch_pte_present(jlos_paging_context_t *ctx, uintptr_t va)
{
    uint64_t *pte = pte_walk(ctx, va);
    return pte && (*pte & JLOS_ARM64_DESC_VALID);
}

uintptr_t jlos_arch_pte_phys(jlos_paging_context_t *ctx, uintptr_t va)
{
    uint64_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_ARM64_DESC_VALID)) {
        return 0;
    }
    return *pte & JLOS_ARM64_PTE_ADDR_MASK;
}

void jlos_arch_pte_set(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    uint64_t *pte = pte_walk(ctx, va);
    if (pte) {
        *pte = (pa & JLOS_ARM64_PTE_ADDR_MASK) | pte_prot(prot);
    }
}

void jlos_arch_pte_set_rw(jlos_paging_context_t *ctx, uintptr_t va)
{
    uint64_t *pte = pte_walk(ctx, va);
    if (pte && (*pte & JLOS_ARM64_DESC_VALID)) {
        *pte &= ~JLOS_ARM64_PTE_AP2_RO;
    }
}

void jlos_arch_pte_make_ro(jlos_paging_context_t *ctx, uintptr_t va)
{
    uint64_t *pte = pte_walk(ctx, va);
    if (pte) {
        *pte |= JLOS_ARM64_PTE_AP2_RO;
    }
}

bool jlos_arch_pte_replace(jlos_paging_context_t *ctx, uintptr_t va, uintptr_t pa, uint32_t prot)
{
    uint64_t *pte = pte_walk(ctx, va);
    if (!pte | !(*pte & JLOS_ARM64_DESC_VALID)) {
        return false;
    }
    *pte = (pa & JLOS_ARM64_PTE_ADDR_MASK) | pte_prot(prot);
    return true;
}

uint32_t jlos_arch_pte_get_prot(jlos_paging_context_t *ctx, uintptr_t va)
{
    uint64_t *pte = pte_walk(ctx, va);
    if (!pte || !(*pte & JLOS_ARM64_DESC_VALID)) {
        return 0;
    }
    uint32_t prot = JLOS_PG_READ;
    if (!(*pte & JLOS_ARM64_PTE_AP2_RO)) {
        prot |= JLOS_PG_WRITE;
    }
    if (*pte & JLOS_ARM64_PTE_AP1_USER) {
        prot |= JLOS_PG_USER;
    }
    if (!(*pte & JLOS_ARM64_PTE_PXN)) {
        prot |= JLOS_PG_EXEC;
    }
    return prot;
}

void jlos_arch_paging_context_tables_destroy(jlos_paging_context_t *ctx)
{
    if (!ctx || !ctx->root) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&ctx->lock);
    uint64_t *l0 = (uint64_t *)ctx->root;
    for (uint32_t i = 0; i < JLOS_ARM64_TABLE_ENTRIES; i++) {
        if (!entry_is_table(l0[i])) {
            continue;;
        }
        uint64_t *l1 = table_of(l0[i]);
        for (uint32_t j = 0; j < JLOS_ARM64_TABLE_ENTRIES; j++) {
            if (!entry_is_table(l1[j])) {
                continue;
            }
            uint64_t *l2 = table_of(l1[j]);
            for (uint32_t k = 0; k < JLOS_ARM64_TABLE_ENTRIES; k++) {
                if (!entry_is_table(l2[k])) {
                    continue;
                }
                uint64_t *l3 = table_of(l2[k]);
                for (uint32_t m = 0; m < JLOS_ARM64_TABLE_ENTRIES; m++) {
                    if (l3[m] & JLOS_ARM64_DESC_VALID) {
                        jlos_page_frame_refcount_dec((uint32_t)(l3[m] & JLOS_ARM64_PTE_ADDR_MASK));
                    }
                }
                jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(l3));
                jlos_page_frame_free(l3);
            }
            jlos_page_frame_clear_owner_type((uint32_t)VIRT_TO_PHYS(l2));
            jlos_page_frame_free(l2);
        }
        jlos_page_frame_clear_owner_type((uint32_t)PHYS_TO_VIRT(l1));
        jlos_page_frame_free(l1);
    }
    jlos_spin_unlock_irqrestore(&ctx->lock, flags);
    jlos_page_frame_free(ctx->root);
    jlos_hal_paging_asid_free(ctx->asid);
    jlos_paging_context_init(ctx);
}

void jlos_arch_paging_context_tables_clone(jlos_paging_context_t *dst, jlos_paging_context_t *src)
{
    (void)src;
    jlos_paging_context_init(dst);
    dst->root = jlos_page_frame_malloc();
    if (!dst->root) {
        return;
    }
    jlos_memset(dst->root, 0, JLOS_ARM64_TABLE_SIZE);
    dst->asid = jlos_hal_paging_asid_alloc();
}

void jlos_arch_paging_initialize_kernel(jlos_paging_table_alloc_fn alloc_fn)
{
    s_page_table_alloc = alloc_fn;
    jlos_paging_context_init(&s_kernel_paging_context);
    s_kernel_l0 = (uint64_t *)(__pgtable_start + JLOS_ARM64_L0_T1_OFF);
    s_kernel_paging_context.root = s_kernel_l0;
    
    __asm__ __volatile__("msr mair_el1, %0" : : "r"(JLOS_ARM64_MAIR_SET) : "memory");
    
    ((uint64_t *)__pgtable_start)[0] = 0;
    jlos_hal_paging_flush_all_tlb();

    jlos_paging_enable(&s_kernel_paging_context);
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
    uint64_t *l0 = (uint64_t *)ctx->root;
    for (uint32_t i = 0; i < JLOS_ARM64_TABLE_ENTRIES; i++) {
        if (!entry_is_table(l0[i])) {
            continue;
        }
        uint64_t *l1 = table_of(l0[i]);
        for (uint32_t j = 0; j < JLOS_ARM64_TABLE_ENTRIES; j++) {
            if (entry_is_block(l1[j])) {
                mapped_pages += JLOS_ARM64_PUD_BLOCK_SIZE >> JLOS_PAGE_SHIFT;
                continue;
            }
            if (!entry_is_table(l1[j])) {
                continue;
            }
            uint64_t *l2 = table_of(l1[j]);
            for (uint32_t k = 0; k < JLOS_ARM64_TABLE_ENTRIES; k++) {
                if (entry_is_block(l2[k])) {
                    mapped_pages += JLOS_ARM64_PMD_BLOCK_SIZE >> JLOS_PAGE_SHIFT;
                    continue;
                }
                if (!entry_is_table(l2[k])) {
                    continue;
                }
                mapped_pages += jlos_page_frame_pt_present_count_get((uint32_t)VIRT_TO_PHYS(table_of(l2[k])));
            }
        }
    }
    jlos_spin_unlock_irqrestore(&ctx->lock, fl);
    printk_info("page states:\n");
    printk_info("- page directory: 0x%lx\n", (unsigned long)ctx->root);
    printk_info("- page tables: %u\n", ctx->num_page_tables);
    printk_info("- mapped pages: %u (%u KB)\n", mapped_pages, mapped_pages * (JLOS_PAGE_SIZE >> 10));
    printk_info("- total physical memory: %u KB\n", jlos_page_frame_get_total() * (JLOS_PAGE_SIZE >> 10));
    printk_info("- free physical memory: %u KB\n", jlos_page_frame_get_free() * (JLOS_PAGE_SIZE >> 10));
}

void jlos_arch_paging_free_boot_tables(void)
{

}
