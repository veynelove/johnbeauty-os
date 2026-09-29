#include <kernel/tests/timek_test.h>
#include <kernel/timek.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_timek"
#include <kernel/printk.h>

static int test_monotonic_increasing(void)
{
    int fail = 0;
    printk_info("[1] monotonic increasing\n");
    uint64_t t1 = jlos_timek_get_monotonic_ns();
    uint64_t t2 = jlos_timek_get_monotonic_ns();
    if (t2 <= t1) {
        printk_err("t2=%llu <= t1=%llu\n",
               (unsigned long long)t2, (unsigned long long)t1);
        fail++;
    } else {
        printk_info("delta=%llu ns\n", (unsigned long long)(t2 - t1));
    }
    return fail;
}

static int test_ticks_non_decreasing(void)
{
    int fail = 0;
    printk_info("[2] ticks non-decreasing\n");
    uint32_t t1 = jlos_timek_get_ticks();
    for (volatile int i = 0; i < 100000; i++) {
    }
    uint32_t t2 = jlos_timek_get_ticks();
    if (t2 < t1) {
        printk_err("t2=%u < t1=%u\n", t2, t1);
        fail++;
    } else {
        printk_info("t1=%u t2=%u\n", t1, t2);
    }
    return fail;
}

static int test_realtime_ge_monotonic(void)
{
    int fail = 0;
    printk_info("[3] realtime >= monotonic\n");
    uint64_t mono = jlos_timek_get_monotonic_ns();
    uint64_t rt = jlos_timek_get_realtime_ns();
    if (rt < mono) {
        printk_err("realtime %llu < monotonic %llu\n",
               (unsigned long long)rt, (unsigned long long)mono);
        fail++;
    } else {
        printk_info("offset=%llu ns\n", (unsigned long long)(rt - mono));
    }
    return fail;
}

void timek_test(void)
{
    int fails = 0;
    printk_info("=== timek test start ===\n");
    fails += test_monotonic_increasing();
    fails += test_ticks_non_decreasing();
    fails += test_realtime_ge_monotonic();
    if (!fails) {
        printk_info("timek: all passed\n");
    } else {
        printk_err("timek: %d failures\n", fails);
    }
}

#if KERNEL_CONFIG_ENABLE_TESTS
JLOS_INITCALL(JLOS_INITCALL_TEST, timek_test);
#endif