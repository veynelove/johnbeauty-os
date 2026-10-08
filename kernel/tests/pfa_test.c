/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_pfa"
#include <kernel/printk.h>

static void fill_frames(void *va, uint32_t bytes, uint8_t seed)
{
    uint8_t *p = (uint8_t *)va;
    for (uint32_t i = 0; i < bytes; i++) {
        p[i] = (uint8_t)(i ^ seed ^ (i >> 8));
    }
}

static bool check_frames(const void *va, uint32_t bytes, uint8_t seed)
{
    const uint8_t *p = (const uint8_t *)va;
    for (uint32_t i = 0; i < bytes; i++) {
        uint8_t expect = (uint8_t)(i ^ seed ^ (i >> 8));
        if (p[i] != expect) {
            return false;
        }
    }
    return true;
}

JLOS_TEST(pfa, single_frame_roundtrip)
{
    const uint32_t N = 64;
    void *frames[N];
    for (uint32_t i = 0; i < N; i++) {
        frames[i] = jlos_page_frame_malloc();
        JLOS_ASSERT_NOT_NULL(frames[i]);
        fill_frames(frames[i], JLOS_PAGE_FRAME_SIZE, (uint8_t)(i & 0xFF));
    }
    for (uint32_t i = 0; i < N; i++) {
        JLOS_TEST_TRUE(check_frames(frames[i], JLOS_PAGE_FRAME_SIZE, (uint8_t)(i & 0xFF)));
        jlos_page_frame_free(frames[i]);
    }
}

JLOS_TEST(pfa, order_alloc)
{
    for (uint32_t order = 0; order <= 3; order++) {
        uint32_t nframes = 1U << order;
        void *va = jlos_page_frame_alloc_order(order);
        JLOS_ASSERT_NOT_NULL(va);
        fill_frames(va, nframes * JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x40 + order));
        JLOS_TEST_TRUE(check_frames(va, nframes * JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x40 + order)));
        jlos_page_frame_free_order(va, order);
    }
}

JLOS_TEST(pfa, alloc_n)
{
    const uint32_t sizes[] = {2, 8, 32};
    for (uint32_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        uint32_t n = sizes[i];
        void *va = jlos_page_frame_alloc_n(n);
        JLOS_ASSERT_NOT_NULL(va);
        uint32_t phys = VIRT_TO_PHYS(va);
        for (uint32_t j = 0; j < n; j++) {
            fill_frames((void *)PHYS_TO_VIRT(phys + j * JLOS_PAGE_FRAME_SIZE),
                        JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x80 + i));
        }
        for (uint32_t j = 0; j < n; j++) {
            JLOS_TEST_TRUE(check_frames((void *)PHYS_TO_VIRT(phys + j * JLOS_PAGE_FRAME_SIZE),
                                        JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x80 + i)));
        }
        jlos_page_frame_free_n(va, n);
    }
}

JLOS_TEST(pfa, refcount)
{
    void *va = jlos_page_frame_malloc();
    JLOS_ASSERT_NOT_NULL(va);
    uint32_t phys = VIRT_TO_PHYS(va);
    JLOS_TEST_EQ(jlos_page_frame_refcount_get(phys), 1);
    jlos_page_frame_refcount_inc(phys);
    JLOS_TEST_EQ(jlos_page_frame_refcount_get(phys), 2);
    jlos_page_frame_refcount_dec(phys);
    JLOS_TEST_EQ(jlos_page_frame_refcount_get(phys), 1);
    jlos_page_frame_free(va);
}

JLOS_TEST(pfa, owner_type)
{
    void *va = jlos_page_frame_malloc();
    JLOS_ASSERT_NOT_NULL(va);
    uint32_t phys = VIRT_TO_PHYS(va);
    uint32_t marker = 0xDEADBEEF;
    jlos_page_frame_set_owner_type(phys, (void *)marker, JLOS_PAGE_FRAME_TYPE_KERN_STACK);
    JLOS_TEST_EQ(jlos_page_frame_get_type(phys), JLOS_PAGE_FRAME_TYPE_KERN_STACK);
    JLOS_TEST_PTR_EQ(jlos_page_frame_get_owner(phys), (void *)marker);
    jlos_page_frame_clear_owner_type(phys);
    JLOS_TEST_EQ(jlos_page_frame_get_type(phys), JLOS_PAGE_FRAME_TYPE_FREE);
    jlos_page_frame_free(va);
}

JLOS_TEST(pfa, pressure_and_accounting)
{
    const uint32_t N = 512;
    void *frames[N];
    uint32_t base_free = jlos_page_frame_get_free();
    for (uint32_t i = 0; i < N; i++) {
        frames[i] = jlos_page_frame_malloc();
        JLOS_ASSERT_NOT_NULL(frames[i]);
    }
    for (uint32_t i = 0; i < N; i++) {
        jlos_page_frame_free(frames[i]);
    }
    JLOS_TEST_EQ(jlos_page_frame_get_free(), base_free);
}
