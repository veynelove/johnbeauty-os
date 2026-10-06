#include <kernel/test.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_mm"
#include <kernel/printk.h>

static void fill_pattern(uint8_t *buf, size_t size, uint8_t seed)
{
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)(i ^ seed ^ ((i >> 8) & 0xFF));
    }
}

static bool check_pattern(const uint8_t *buf, size_t size, uint8_t seed, const char *ctx)
{
    for (size_t i = 0; i < size; i++) {
        uint8_t expect = (uint8_t)(i ^ seed ^ ((i >> 8) & 0xFF));
        if (buf[i] != expect) {
            printk_err("mismatch [%s] i=%u got=0x%x expect=0x%x\n",
                   ctx, (unsigned)i, (unsigned)buf[i], (unsigned)expect);
            return false;
        }
    }
    return true;
}

JLOS_TEST(memory, boundary)
{
    void *p0 = jlos_kalloc(0);
    JLOS_TEST_NULL(p0);
    jlos_kfree(p0);

    const size_t cases[] = {1, 15, 16, 17, 31, 32, 33,
                            JLOS_MM_MIN_ALLOC, JLOS_MM_MIN_ALLOC + 1,
                            JLOS_PAGE_FRAME_SIZE - 1, JLOS_PAGE_FRAME_SIZE,
                            JLOS_PAGE_FRAME_SIZE + 1};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t sz = cases[i];
        uint8_t *p = (uint8_t *)jlos_kalloc(sz);
        JLOS_ASSERT_NOT_NULL(p);
        fill_pattern(p, sz, (uint8_t)(sz & 0xFF));
        JLOS_TEST_TRUE(check_pattern(p, sz, (uint8_t)(sz & 0xFF), "boundary"));
        jlos_kfree(p);
    }
}

JLOS_TEST(memory, small_slab)
{
    const size_t cases[] = {16, 32, 64, 128, 256, 512, 1024, 2048,
                            JLOS_PAGE_FRAME_SIZE - sizeof(jlos_memory_slab_page_t)};
    const size_t N = sizeof(cases) / sizeof(cases[0]);
    void *ptrs[N];
    for (size_t i = 0; i < N; i++) {
        ptrs[i] = jlos_kalloc(cases[i]);
        JLOS_ASSERT_NOT_NULL(ptrs[i]);
        fill_pattern((uint8_t *)ptrs[i], cases[i], (uint8_t)(0xA5 + i));
    }
    for (size_t i = 0; i < N; i++) {
        JLOS_TEST_TRUE(check_pattern((const uint8_t *)ptrs[i], cases[i],
                                      (uint8_t)(0xA5 + i), "slab"));
        jlos_kfree(ptrs[i]);
    }
}

JLOS_TEST(memory, large_contig)
{
    const size_t cases[] = {JLOS_PAGE_FRAME_SIZE, 4 * JLOS_PAGE_FRAME_SIZE,
                            16 * JLOS_PAGE_FRAME_SIZE, 64 * JLOS_PAGE_FRAME_SIZE};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t sz = cases[i];
        uint8_t *p = (uint8_t *)jlos_kalloc(sz);
        JLOS_ASSERT_NOT_NULL(p);
        fill_pattern(p, sz, (uint8_t)(0x11 + i * 0x22));
        JLOS_TEST_TRUE(check_pattern(p, sz, (uint8_t)(0x11 + i * 0x22), "contig"));
        jlos_kfree(p);
    }
}

JLOS_TEST(memory, kvheap_fallback)
{
    const size_t cases[] = {96, 200, 500, 1500, 2500};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t sz = cases[i];
        uint8_t *p = (uint8_t *)jlos_kvalloc(sz);
        JLOS_ASSERT_NOT_NULL(p);
        fill_pattern(p, sz, (uint8_t)(0x5A + i));
        JLOS_TEST_TRUE(check_pattern(p, sz, (uint8_t)(0x5A + i), "kvheap"));
        jlos_kvfree(p);
    }
}

JLOS_TEST(memory, roundtrip)
{
    const unsigned N = 1000;
    const size_t SZ = 128;
    for (unsigned n = 0; n < N; n++) {
        uint8_t *p = (uint8_t *)jlos_kalloc(SZ);
        JLOS_ASSERT_NOT_NULL(p);
        fill_pattern(p, SZ, (uint8_t)(0x3C + (n & 0xFF)));
        JLOS_TEST_TRUE(check_pattern(p, SZ, (uint8_t)(0x3C + (n & 0xFF)), "rt-slab"));
        jlos_kfree(p);
    }
    const unsigned M = 100;
    const size_t BIG = 16 * JLOS_PAGE_FRAME_SIZE;
    for (unsigned n = 0; n < M; n++) {
        uint8_t *p = (uint8_t *)jlos_kalloc(BIG);
        JLOS_ASSERT_NOT_NULL(p);
        fill_pattern(p, BIG, (uint8_t)(0x77 + (n & 0xFF)));
        JLOS_TEST_TRUE(check_pattern(p, BIG, (uint8_t)(0x77 + (n & 0xFF)), "rt-contig"));
        jlos_kfree(p);
    }
}
