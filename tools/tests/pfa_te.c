#include <tools/tests/pfa_te.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"

static void fill_frames(void *va, uint32_t bytes, uint8_t seed)
{
    uint8_t *p = (uint8_t *)va;
    for (uint32_t i = 0; i < bytes; i++)
        p[i] = (uint8_t)(i ^ seed ^ (i >> 8));
}

static int check_frames(const void *va, uint32_t bytes, uint8_t seed)
{
    const uint8_t *p = (const uint8_t *)va;
    for (uint32_t i = 0; i < bytes; i++) {
        uint8_t expect = (uint8_t)(i ^ seed ^ (i >> 8));
        if (p[i] != expect)
            return 1;
    }
    return 0;
}

static int test_single_frame_roundtrip(void)
{
    const uint32_t N = 64;
    void *frames[N];
    int fail = 0;

    printk_info("[test 1] single frame roundtrip x %u\n", N);
    for (uint32_t i = 0; i < N; i++) {
        frames[i] = jlos_page_frame_malloc();
        if (!frames[i]) {
            printk_err("FAIL: malloc #%u NULL\n", i);
            fail++;
            continue;
        }
        fill_frames(frames[i], JLOS_PAGE_FRAME_SIZE, (uint8_t)(i & 0xFF));
    }
    for (uint32_t i = 0; i < N; i++) {
        if (!frames[i])
            continue;
        if (check_frames(frames[i], JLOS_PAGE_FRAME_SIZE, (uint8_t)(i & 0xFF))) {
            printk_err("FAIL: frame #%u corrupted\n", i);
            fail++;
        }
        jlos_page_frame_free(frames[i]);
    }
    if (!fail)
        printk_info("OK: %u frames alloc/write/verify/free\n", N);
    return fail;
}

static int test_order_alloc(void)
{
    int fail = 0;

    printk_info("[test 2] order alloc/free\n");
    for (uint32_t order = 0; order <= 3; order++) {
        uint32_t nframes = 1U << order;
        void *va = jlos_page_frame_alloc_order(order);
        if (!va) {
            printk_err("FAIL: alloc_order(%u) NULL\n", order);
            fail++;
            continue;
        }
        fill_frames(va, nframes * JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x40 + order));
        if (check_frames(va, nframes * JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x40 + order))) {
            printk_err("FAIL: order(%u) head corrupted\n", order);
            fail++;
        }
        jlos_page_frame_free_order(va, order);
        printk_info("OK: order %u (%u frames)\n", order, nframes);
    }
    return fail;
}

static int test_reserve_bulk(void)
{
    const uint32_t sizes[] = {2, 8, 32};
    int fail = 0;

    printk_info("[test 3] reserve/free bulk\n");
    for (uint32_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        uint32_t n = sizes[i];
        void *va = jlos_page_frame_reserve_bulk(n);
        if (!va) {
            printk_err("FAIL: reserve_bulk(%u) NULL\n", n);
            fail++;
            continue;
        }
        uint32_t phys = VIRT_TO_PHYS(va);
        for (uint32_t j = 0; j < n; j++)
            fill_frames((void *)PHYS_TO_VIRT(phys + j * JLOS_PAGE_FRAME_SIZE),
                        JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x80 + i));
        for (uint32_t j = 0; j < n; j++) {
            if (check_frames((void *)PHYS_TO_VIRT(phys + j * JLOS_PAGE_FRAME_SIZE),
                             JLOS_PAGE_FRAME_SIZE, (uint8_t)(0x80 + i))) {
                printk_err("FAIL: bulk page %u corrupted\n", j);
                fail++;
            }
        }
        jlos_page_frame_free_bulk(phys, n);
        printk_info("OK: bulk %u frames\n", n);
    }
    return fail;
}

static int test_refcount(void)
{
    int fail = 0;

    printk_info("[test 4] refcount semantics\n");
    void *va = jlos_page_frame_malloc();
    if (!va) {
        printk_err("FAIL: malloc NULL\n");
        return 1;
    }
    uint32_t phys = VIRT_TO_PHYS(va);
    if (jlos_page_frame_refcount_get(phys) != 1) {
        printk_err("FAIL: initial refcount != 1\n");
        fail++;
    }
    jlos_page_frame_refcount_inc(phys);
    if (jlos_page_frame_refcount_get(phys) != 2) {
        printk_err("FAIL: refcount after inc != 2\n");
        fail++;
    }
    jlos_page_frame_refcount_dec(phys);
    if (jlos_page_frame_refcount_get(phys) != 1) {
        printk_err("FAIL: refcount after dec != 1\n");
        fail++;
    }
    jlos_page_frame_free(va);
    if (!fail)
        printk_info("OK: inc/dec/get consistent\n");
    return fail;
}

static int test_owner_type(void)
{
    int fail = 0;

    printk_info("[test 5] owner type mark\n");
    void *va = jlos_page_frame_malloc();
    if (!va) {
        printk_err("FAIL: malloc NULL\n");
        return 1;
    }
    uint32_t phys = VIRT_TO_PHYS(va);
    uint32_t marker = 0xDEADBEEF;
    jlos_page_frame_set_owner_type(phys, (void *)marker, JLOS_PAGE_FRAME_TYPE_KERN_STACK);
    if (jlos_page_frame_get_type(phys) != JLOS_PAGE_FRAME_TYPE_KERN_STACK) {
        printk_err("FAIL: type != KERN_STACK\n");
        fail++;
    }
    if (jlos_page_frame_get_owner(phys) != (void *)marker) {
        printk_err("FAIL: owner != marker\n");
        fail++;
    }
    jlos_page_frame_clear_owner_type(phys);
    if (jlos_page_frame_get_type(phys) != JLOS_PAGE_FRAME_TYPE_FREE) {
        printk_err("FAIL: type after clear != FREE\n");
        fail++;
    }
    jlos_page_frame_free(va);
    if (!fail)
        printk_info("OK: set/get/clear owner\n");
    return fail;
}

static int test_pressure_and_accounting(void)
{
    const uint32_t N = 512;
    void *frames[N];
    int fail = 0;

    printk_info("[test 6] pressure x %u + free accounting\n", N);
    uint32_t base_free = jlos_page_frame_get_free();
    for (uint32_t i = 0; i < N; i++) {
        frames[i] = jlos_page_frame_malloc();
        if (!frames[i]) {
            printk_err("FAIL: malloc #%u NULL\n", i);
            fail++;
            break;
        }
    }
    uint32_t mid_free = jlos_page_frame_get_free();
    for (uint32_t i = 0; i < N; i++) {
        if (frames[i])
            jlos_page_frame_free(frames[i]);
    }
    uint32_t end_free = jlos_page_frame_get_free();
    printk_info("free: base=%u mid=%u end=%u\n", base_free, mid_free, end_free);
    if (end_free != base_free) {
        printk_err("FAIL: free not restored (%u -> %u)\n", base_free, end_free);
        fail++;
    }
    if (!fail)
        printk_info("OK: free accounting restored\n");
    return fail;
}

void pfa_test(void)
{
    printk_info("=== pfa test start ===\n");
    printk_info("total=%u frames free=%u\n",
           jlos_page_frame_get_total(), jlos_page_frame_get_free());

    int fails = 0;
    fails += test_single_frame_roundtrip();
    fails += test_order_alloc();
    fails += test_reserve_bulk();
    fails += test_refcount();
    fails += test_owner_type();
    fails += test_pressure_and_accounting();

    if (!fails)
        printk_info("pfa: ALL PASSED\n");
    else
        printk_err("pfa: FAILS: %d, check above\n", fails);
}