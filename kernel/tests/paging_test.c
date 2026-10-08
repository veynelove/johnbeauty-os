/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <hal/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_paging"
#include <kernel/printk.h>

#define TEST_VBASE 0x10000000

JLOS_TEST(paging, map_unmap)
{
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);
    void *frame = jlos_page_frame_malloc();
    JLOS_ASSERT_NOT_NULL(frame);
    uint32_t phys = VIRT_TO_PHYS(frame);
    uint32_t va = TEST_VBASE;

    JLOS_TEST_TRUE(jlos_paging_map(&ctx, va, phys, JLOS_PG_USER_RW));
    JLOS_TEST_EQ(jlos_paging_get_physical_addr(&ctx, va), phys);
    JLOS_TEST_TRUE(jlos_arch_pte_present(&ctx, va));

    JLOS_TEST_TRUE(jlos_paging_unmap(&ctx, va));
    JLOS_TEST_EQ(jlos_paging_get_physical_addr(&ctx, va), 0);

    jlos_arch_paging_context_tables_destroy(&ctx);
}

JLOS_TEST(paging, map_range)
{
    const uint32_t N = 3;
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);
    void *seed = jlos_page_frame_malloc();
    JLOS_ASSERT_NOT_NULL(seed);
    jlos_paging_map(&ctx, TEST_VBASE, VIRT_TO_PHYS(seed), JLOS_PG_USER_RW);

    void *bulk = jlos_page_frame_alloc_n(N);
    JLOS_ASSERT_NOT_NULL(bulk);
    uint32_t phys = VIRT_TO_PHYS(bulk);
    uint32_t va = TEST_VBASE + JLOS_PAGE_FRAME_SIZE;

    JLOS_TEST_TRUE(jlos_paging_map_range(&ctx, va, phys, N * JLOS_PAGE_FRAME_SIZE, JLOS_PG_USER_RW));
    for (uint32_t i = 0; i < N; i++) {
        JLOS_TEST_EQ(jlos_paging_get_physical_addr(&ctx, va + i * JLOS_PAGE_FRAME_SIZE),
                     phys + i * JLOS_PAGE_FRAME_SIZE);
    }
    jlos_paging_unmap(&ctx, TEST_VBASE);
    for (uint32_t i = 0; i < N; i++) {
        jlos_paging_unmap(&ctx, va + i * JLOS_PAGE_FRAME_SIZE);
    }
    jlos_arch_paging_context_tables_destroy(&ctx);
}

JLOS_TEST(paging, change_flags)
{
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);
    void *frame = jlos_page_frame_malloc();
    JLOS_ASSERT_NOT_NULL(frame);
    uint32_t phys = VIRT_TO_PHYS(frame);
    uint32_t va = TEST_VBASE + 2 * JLOS_PAGE_FRAME_SIZE;

    jlos_paging_map(&ctx, va, phys, JLOS_PG_USER_RW);
    uint32_t prot = jlos_arch_pte_get_prot(&ctx, va);
    JLOS_TEST_TRUE(prot & JLOS_PG_READ);
    JLOS_TEST_TRUE(prot & JLOS_PG_USER);
    JLOS_TEST_TRUE(prot & JLOS_PG_WRITE);

    jlos_paging_change_flags(&ctx, va, JLOS_PG_USER_RO);
    prot = jlos_arch_pte_get_prot(&ctx, va);
    JLOS_TEST_TRUE(prot & JLOS_PG_READ);
    JLOS_TEST_TRUE(prot & JLOS_PG_USER);
    JLOS_TEST_FALSE(prot & JLOS_PG_WRITE);

    jlos_paging_unmap(&ctx, va);
    jlos_arch_paging_context_tables_destroy(&ctx);
}

JLOS_TEST(paging, clone)
{
    jlos_paging_context_t src, dst;
    jlos_paging_context_init(&src);
    void *frame = jlos_page_frame_malloc();
    JLOS_ASSERT_NOT_NULL(frame);
    uint32_t va = TEST_VBASE + 3 * JLOS_PAGE_FRAME_SIZE;
    jlos_paging_map(&src, va, VIRT_TO_PHYS(frame), JLOS_PG_USER_RW);

    jlos_arch_paging_context_tables_clone(&dst, &src);
    JLOS_TEST_NOT_NULL(dst.root);
    JLOS_TEST_EQ(jlos_paging_get_physical_addr(&dst, va), 0);

    jlos_arch_paging_context_tables_destroy(&dst);
    jlos_paging_unmap(&src, va);
    jlos_arch_paging_context_tables_destroy(&src);
}

JLOS_TEST(paging, user_accessible)
{
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);
    void *frame = jlos_page_frame_malloc();
    JLOS_ASSERT_NOT_NULL(frame);
    jlos_paging_map(&ctx, TEST_VBASE, VIRT_TO_PHYS(frame), JLOS_PG_USER_RW);

    JLOS_TEST_TRUE(jlos_paging_is_user_accessible(&ctx, TEST_VBASE, 4));
    JLOS_TEST_FALSE(jlos_paging_is_user_accessible(&ctx, KERNEL_VIRTUAL_BASE, 4));
    JLOS_TEST_FALSE(jlos_paging_is_user_accessible(&ctx, KERNEL_VIRTUAL_BASE - 8, 16));

    jlos_paging_unmap(&ctx, TEST_VBASE);
    jlos_arch_paging_context_tables_destroy(&ctx);
}
