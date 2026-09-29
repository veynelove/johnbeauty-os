#include <kernel/tests/paging_test.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <hal/paging.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_paging"
#include <kernel/printk.h>

#define TEST_VBASE 0x10000000

static int test_map_unmap(void)
{
    int fail = 0;
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);

    printk_info("[test 1] map/unmap roundtrip\n");
    void *frame = jlos_page_frame_malloc();
    if (!frame) {
        printk_err("frame malloc NULL\n");
        return 1;
    }
    uint32_t phys = VIRT_TO_PHYS(frame);
    uint32_t va = TEST_VBASE;

    if (!jlos_paging_map(&ctx, va, phys, JLOS_PG_USER_RW)) {
        printk_err("map failed\n");
        fail++;
    }
    if (jlos_paging_get_physical_addr(&ctx, va) != phys) {
        printk_err("get_phys != mapped phys\n");
        fail++;
    }
    if (!jlos_arch_pte_present(&ctx, va)) {
        printk_err("PTE not present\n");
        fail++;
    }

    if (!jlos_paging_unmap(&ctx, va)) {
        printk_err("unmap failed\n");
        fail++;
    }
    if (jlos_paging_get_physical_addr(&ctx, va) != 0) {
        printk_err("get_phys after unmap != 0\n");
        fail++;
    }

    jlos_arch_paging_context_tables_destroy(&ctx);
    if (!fail)
        printk_info("map -> get -> unmap -> get(0)\n");
    return fail;
}

static int test_map_range(void)
{
    const uint32_t N = 3;
    int fail = 0;
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);

    printk_info("[test 2] map_range %u pages\n", N);
    void *seed = jlos_page_frame_malloc();
    if (!seed) {
        printk_err("seed malloc NULL\n");
        return 1;
    }
    jlos_paging_map(&ctx, TEST_VBASE, VIRT_TO_PHYS(seed), JLOS_PG_USER_RW);

    void *bulk = jlos_page_frame_alloc_n(N);
    if (!bulk) {
        printk_err("alloc_n NULL\n");
        jlos_paging_unmap(&ctx, TEST_VBASE);
        jlos_arch_paging_context_tables_destroy(&ctx);
        return 1;
    }
    uint32_t phys = VIRT_TO_PHYS(bulk);
    uint32_t va = TEST_VBASE + JLOS_PAGE_FRAME_SIZE;

    if (!jlos_paging_map_range(&ctx, va, phys, N * JLOS_PAGE_FRAME_SIZE, JLOS_PG_USER_RW)) {
        printk_err("map_range failed\n");
        fail++;
    }
    for (uint32_t i = 0; i < N; i++) {
        if (jlos_paging_get_physical_addr(&ctx, va + i * JLOS_PAGE_FRAME_SIZE)
            != phys + i * JLOS_PAGE_FRAME_SIZE) {
            printk_err("page %u phys mismatch\n", i);
            fail++;
        }
    }
    jlos_paging_unmap(&ctx, TEST_VBASE);
    for (uint32_t i = 0; i < N; i++)
        jlos_paging_unmap(&ctx, va + i * JLOS_PAGE_FRAME_SIZE);

    jlos_arch_paging_context_tables_destroy(&ctx);
    if (!fail)
        printk_info("%u pages mapped/unmapped\n", N);
    return fail;
}

static int test_change_flags(void)
{
    int fail = 0;
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);

    printk_info("[test 3] change_flags\n");
    void *frame = jlos_page_frame_malloc();
    if (!frame) {
        printk_err("frame malloc NULL\n");
        return 1;
    }
    uint32_t phys = VIRT_TO_PHYS(frame);
    uint32_t va = TEST_VBASE + 2 * JLOS_PAGE_FRAME_SIZE;

    jlos_paging_map(&ctx, va, phys, JLOS_PG_USER_RW);
    uint32_t prot = jlos_arch_pte_get_prot(&ctx, va);
    if (!(prot & JLOS_PG_READ) || !(prot & JLOS_PG_USER) || !(prot & JLOS_PG_WRITE)) {
        printk_err("initial PTE flags wrong (0x%x)\n", prot);
        fail++;
    }

    jlos_paging_change_flags(&ctx, va, JLOS_PG_USER_RO);
    prot = jlos_arch_pte_get_prot(&ctx, va);
    if (!(prot & JLOS_PG_READ) || !(prot & JLOS_PG_USER)) {
        printk_err("RO PTE lost READ/USER (0x%x)\n", prot);
        fail++;
    }
    if (prot & JLOS_PG_WRITE) {
        printk_err("RO PTE still writable\n");
        fail++;
    }

    jlos_paging_unmap(&ctx, va);
    jlos_arch_paging_context_tables_destroy(&ctx);
    if (!fail)
        printk_info("RW -> RO flag transition\n");
    return fail;
}

static int test_clone(void)
{
    int fail = 0;
    jlos_paging_context_t src, dst;
    jlos_paging_context_init(&src);

    printk_info("[test 4] context clone\n");
    void *frame = jlos_page_frame_malloc();
    if (!frame) {
        printk_err("frame malloc NULL\n");
        return 1;
    }
    uint32_t va = TEST_VBASE + 3 * JLOS_PAGE_FRAME_SIZE;
    jlos_paging_map(&src, va, VIRT_TO_PHYS(frame), JLOS_PG_USER_RW);

    jlos_arch_paging_context_tables_clone(&dst, &src);

    if (!dst.root) {
        printk_err("clone root NULL\n");
        fail++;
    }
    if (jlos_paging_get_physical_addr(&dst, va) != 0) {
        printk_err("user mapping leaked to clone\n");
        fail++;
    }

    jlos_arch_paging_context_tables_destroy(&dst);
    jlos_paging_unmap(&src, va);
    jlos_arch_paging_context_tables_destroy(&src);
    if (!fail)
        printk_info("user mapping not cloned\n");
    return fail;
}

static int test_user_accessible(void)
{
    int fail = 0;
    jlos_paging_context_t ctx;
    jlos_paging_context_init(&ctx);

    printk_info("[test 5] access_ok boundary\n");
    void *frame = jlos_page_frame_malloc();
    if (!frame) {
        printk_err("frame malloc NULL\n");
        return 1;
    }
    jlos_paging_map(&ctx, TEST_VBASE, VIRT_TO_PHYS(frame), JLOS_PG_USER_RW);

    if (!jlos_paging_is_user_accessible(&ctx, TEST_VBASE, 4)) {
        printk_err("user addr range rejected\n");
        fail++;
    }
    if (jlos_paging_is_user_accessible(&ctx, KERNEL_VIRTUAL_BASE, 4)) {
        printk_err("kernel addr range accepted\n");
        fail++;
    }
    if (jlos_paging_is_user_accessible(&ctx, KERNEL_VIRTUAL_BASE - 8, 16)) {
        printk_err("overflowing range accepted\n");
        fail++;
    }

    jlos_paging_unmap(&ctx, TEST_VBASE);
    jlos_arch_paging_context_tables_destroy(&ctx);
    if (!fail)
        printk_info("user<->kernel boundary enforced\n");
    return fail;
}

void paging_test(void)
{
    printk_info("=== paging test start ===\n");

    int fails = 0;
    fails += test_map_unmap();
    fails += test_map_range();
    fails += test_change_flags();
    fails += test_clone();
    fails += test_user_accessible();

    if (!fails)
        printk_info("paging: all passed\n");
    else
        printk_err("paging: %d failures\n", fails);
}

#if KERNEL_CONFIG_ENABLE_TESTS
JLOS_INITCALL(JLOS_INITCALL_TEST, paging_test);
#endif
