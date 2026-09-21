/* tools/tests/memory_te.c — JLOS kalloc/kfree 统一入口测试
 * 覆盖：边界 / SLAB small / 连续物理 large / kv-heap fallback / roundtrip */
#include <tools/tests/memory_te.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"
#include <kernel/printk.h>

/* buf[i] = i ^ seed ^ (i>>8 & 0xFF) */
static void fill_pattern(uint8_t *buf, size_t size, uint8_t seed)
{
    for (size_t i = 0; i < size; i++)
        buf[i] = (uint8_t)(i ^ seed ^ ((i >> 8) & 0xFF));
}

/* pattern 比对，0=OK；不一致时打印首个错位点并返回 1 */
static int check_pattern(const uint8_t *buf, size_t size, uint8_t seed, const char *ctx)
{
    for (size_t i = 0; i < size; i++) {
        uint8_t expect = (uint8_t)(i ^ seed ^ ((i >> 8) & 0xFF));
        if (buf[i] != expect) {
            printk_err("mismatch [%s] i=%u got=0x%x expect=0x%x\n",
                   ctx, (unsigned)i, (unsigned)buf[i], (unsigned)expect);
            return 1;
        }
    }
    return 0;
}

/* ---- TEST 1: 边界 ---- */
static int test_boundary(void)
{
    int fail = 0;
    printk_info("[test 1] boundary\n");

    void *p0 = jlos_kalloc(0);
    if (p0) {
        printk_err("FAIL: kalloc(0)=%p want NULL\n", p0);
        fail++;
    } else {
        printk_info("OK: kalloc(0) == NULL\n");
    }
    jlos_kfree(p0);

    const size_t cases[] = {1, 15, 16, 17, 31, 32, 33,
                            JLOS_MM_MIN_ALLOC, JLOS_MM_MIN_ALLOC + 1,
                            JLOS_PAGE_FRAME_SIZE - 1, JLOS_PAGE_FRAME_SIZE,
                            JLOS_PAGE_FRAME_SIZE + 1};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t sz = cases[i];
        uint8_t *p = (uint8_t *)jlos_kalloc(sz);
        if (!p) {
            printk_err("FAIL: kalloc(%u)=NULL\n", (unsigned)sz);
            fail++;
            continue;
        }
        fill_pattern(p, sz, (uint8_t)(sz & 0xFF));
        if (check_pattern(p, sz, (uint8_t)(sz & 0xFF), "boundary"))
            fail++;
        jlos_kfree(p);
    }
    printk_info("OK: boundary (%u cases)\n",
           (unsigned)(sizeof(cases) / sizeof(cases[0])));
    return fail;
}

/* ---- TEST 2: SLAB small 多桶 (先全分配再释放, 暴露重叠/双 free) ---- */
static int test_small_slab(void)
{
    int fail = 0;
    printk_info("[test 2] small slab\n");

    const size_t cases[] = {16, 32, 64, 128, 256, 512, 1024, 2048,
                            JLOS_PAGE_FRAME_SIZE - sizeof(jlos_memory_slab_page_t)};
    const size_t N = sizeof(cases) / sizeof(cases[0]);
    void *ptrs[N];

    for (size_t i = 0; i < N; i++) {
        ptrs[i] = jlos_kalloc(cases[i]);
        if (!ptrs[i]) {
            printk_err("FAIL: kalloc(%u)=NULL\n", (unsigned)cases[i]);
            fail++;
            continue;
        }
        fill_pattern((uint8_t *)ptrs[i], cases[i], (uint8_t)(0xA5 + i));
    }
    for (size_t i = 0; i < N; i++) {
        if (!ptrs[i])
            continue;
        if (check_pattern((const uint8_t *)ptrs[i], cases[i],
                          (uint8_t)(0xA5 + i), "slab"))
            fail++;
        jlos_kfree(ptrs[i]);
    }
    printk_info("OK: small slab (%u sizes)\n", (unsigned)N);
    return fail;
}

/* ---- TEST 3: 连续物理 large (kvalloc path A) ---- */
static int test_large_contig(void)
{
    int fail = 0;
    printk_info("[test 3] large contig\n");

    const size_t cases[] = {JLOS_PAGE_FRAME_SIZE, 4 * JLOS_PAGE_FRAME_SIZE,
                            16 * JLOS_PAGE_FRAME_SIZE, 64 * JLOS_PAGE_FRAME_SIZE};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t sz = cases[i];
        uint8_t *p = (uint8_t *)jlos_kalloc(sz);
        if (!p) {
            printk_warn("SKIP: kalloc(%u)=NULL (OOM)\n", (unsigned)sz);
            continue;
        }
        fill_pattern(p, sz, (uint8_t)(0x11 + i * 0x22));
        if (check_pattern(p, sz, (uint8_t)(0x11 + i * 0x22), "contig"))
            fail++;
        jlos_kfree(p);
        printk_info("OK: %u pages\n", (unsigned)(sz / JLOS_PAGE_FRAME_SIZE));
    }
    return fail;
}

/* ---- TEST 4: kvalloc < PAGE_SIZE 强制走虚拟堆 (path B, 补 coverage) ---- */
static int test_kvheap_fallback(void)
{
    int fail = 0;
    printk_info("[test 4] kvalloc small -> heap fallback\n");

    const size_t cases[] = {96, 200, 500, 1500, 2500};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t sz = cases[i];
        uint8_t *p = (uint8_t *)jlos_kvalloc(sz);
        if (!p) {
            printk_err("FAIL: kvalloc(%u)=NULL\n", (unsigned)sz);
            fail++;
            continue;
        }
        fill_pattern(p, sz, (uint8_t)(0x5A + i));
        if (check_pattern(p, sz, (uint8_t)(0x5A + i), "kvheap"))
            fail++;
        jlos_kvfree(p);
    }
    printk_info("OK: kvheap fallback\n");
    return fail;
}

/* ---- TEST 5: roundtrip 压力 ---- */
static int test_roundtrip(void)
{
    int fail = 0;
    const unsigned N = 1000;
    const size_t SZ = 128;
    printk_info("[test 5] slab roundtrip: kalloc(%u) x %u\n", (unsigned)SZ, N);

    for (unsigned n = 0; n < N; n++) {
        uint8_t *p = (uint8_t *)jlos_kalloc(SZ);
        if (!p) {
            printk_err("FAIL: iter %u NULL\n", n);
            fail++;
            break;
        }
        fill_pattern(p, SZ, (uint8_t)(0x3C + (n & 0xFF)));
        if (check_pattern(p, SZ, (uint8_t)(0x3C + (n & 0xFF)), "rt-slab")) {
            fail++;
            jlos_kfree(p);
            break;
        }
        jlos_kfree(p);
    }
    if (!fail)
        printk_info("OK: %u iters\n", N);

    const unsigned M = 100;
    const size_t BIG = 16 * JLOS_PAGE_FRAME_SIZE;
    printk_info("[test 5b] contig roundtrip: %u pages x %u\n",
           (unsigned)(BIG / JLOS_PAGE_FRAME_SIZE), M);
    fail = 0;
    for (unsigned n = 0; n < M; n++) {
        uint8_t *p = (uint8_t *)jlos_kalloc(BIG);
        if (!p) {
            printk_err("FAIL: iter %u NULL\n", n);
            fail++;
            break;
        }
        fill_pattern(p, BIG, (uint8_t)(0x77 + (n & 0xFF)));
        if (check_pattern(p, BIG, (uint8_t)(0x77 + (n & 0xFF)), "rt-contig")) {
            fail++;
            jlos_kfree(p);
            break;
        }
        jlos_kfree(p);
    }
    if (!fail)
        printk_info("OK: %u iters\n", M);
    return fail;
}

/* ---- 主入口 ---- */
void memory_manager_test(const void *multiboot_structure)
{
    (void)multiboot_structure;

    printk_info("=== memory test start ===\n");
    printk_info("MIN_ALLOC=%uB CLASS_COUNT=%u PAGE=%uB\n",
           (unsigned)JLOS_MM_MIN_ALLOC, (unsigned)JLOS_MM_CLASS_COUNT,
           (unsigned)JLOS_PAGE_FRAME_SIZE);
    printk_info("PFA total=%u frames (~%u KB) max slab small=%uB\n",
           (unsigned)jlos_page_frame_get_total(),
           (unsigned)jlos_page_frame_get_total() * (JLOS_PAGE_FRAME_SIZE / 1024),
           (unsigned)(JLOS_PAGE_FRAME_SIZE - sizeof(jlos_memory_slab_page_t)));

    int fails = 0;
    fails += test_boundary();
    fails += test_small_slab();
    fails += test_large_contig();
    fails += test_kvheap_fallback();
    fails += test_roundtrip();

    if (!fails)
        printk_info("memory: ALL PASSED\n");
    else
        printk_err("memory: FAILS: %d, check above\n", fails);

    printk_info("post-test heap stats:\n");
    jlos_kvalloc_stats(jlos_active_memory_manager);
}
