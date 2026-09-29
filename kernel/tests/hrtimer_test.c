#include <kernel/tests/hrtimer_test.h>
#include <kernel/hrtimer.h>
#include <kernel/timek.h>
#include <dsa/rbtree.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_hrtm"
#include <kernel/printk.h>

static int test_callback(jlos_hrtimer_t *t)
{
    (void)t;
    return 0;
}

static int test_lifecycle(void)
{
    int fail = 0;
    printk_info("[1] init/start/cancel lifecycle\n");

    jlos_hrtimer_base_t base;
    jlos_hrtimer_base_init(&base);

    jlos_hrtimer_t timer;
    jlos_hrtimer_init(&timer, test_callback, NULL);

    if (timer.active) {
        printk_err("init: active == true\n");
        fail++;
    }
    if (timer.expires != 0) {
        printk_err("init: expires != 0\n");
        fail++;
    }

    jlos_hrtimer_start(&base, &timer, 1000000, 0, JLOS_HRTIMER_MODE_REL);
    if (!timer.active) {
        printk_err("start: active == false\n");
        fail++;
    }
    if (timer.interval != 0) {
        printk_err("start: interval != 0\n");
        fail++;
    }
    if (timer.expires == 0) {
        printk_err("start: expires == 0\n");
        fail++;
    }

    jlos_hrtimer_cancel(&base, &timer);
    if (timer.active) {
        printk_err("cancel: active == true\n");
        fail++;
    }

    if (!fail) {
        printk_info("lifecycle ok\n");
    }
    return fail;
}

static int test_periodic(void)
{
    int fail = 0;
    printk_info("[2] periodic interval\n");

    jlos_hrtimer_base_t base;
    jlos_hrtimer_base_init(&base);

    jlos_hrtimer_t timer;
    jlos_hrtimer_init(&timer, test_callback, NULL);
    jlos_hrtimer_start(&base, &timer, 1000000, 500000, JLOS_HRTIMER_MODE_REL);

    if (timer.interval != 500000) {
        printk_err("interval: got %llu want 500000\n",
               (unsigned long long)timer.interval);
        fail++;
    }

    jlos_hrtimer_cancel(&base, &timer);
    if (!fail) {
        printk_info("periodic interval ok\n");
    }
    return fail;
}

static int test_ordering(void)
{
    int fail = 0;
    printk_info("[3] ordering by expires\n");

    jlos_hrtimer_base_t base;
    jlos_hrtimer_base_init(&base);

    jlos_hrtimer_t t1, t2, t3;
    jlos_hrtimer_init(&t1, test_callback, NULL);
    jlos_hrtimer_init(&t2, test_callback, NULL);
    jlos_hrtimer_init(&t3, test_callback, NULL);

    jlos_hrtimer_start(&base, &t1, 300000000, 0, JLOS_HRTIMER_MODE_REL);
    jlos_hrtimer_start(&base, &t2, 100000000, 0, JLOS_HRTIMER_MODE_REL);
    jlos_hrtimer_start(&base, &t3, 500000000, 0, JLOS_HRTIMER_MODE_REL);

    jlos_rbtree_node_t *first = jlos_rbtree_first(&base.active);
    if (!first) {
        printk_err("rbtree_first NULL\n");
        fail++;
    } else {
        jlos_hrtimer_t *earliest = jlos_rbtree_entry(first, jlos_hrtimer_t, node);
        if (earliest != &t2) {
            printk_err("first expires=%llu, want t2 (100ms)\n",
                   (unsigned long long)earliest->expires);
            fail++;
        } else {
            printk_info("earliest is t2 (100ms)\n");
        }
    }

    jlos_hrtimer_cancel(&base, &t1);
    jlos_hrtimer_cancel(&base, &t2);
    jlos_hrtimer_cancel(&base, &t3);
    if (!fail) {
        printk_info("ordering ok\n");
    }
    return fail;
}

void hrtimer_test(void)
{
    int fails = 0;
    printk_info("=== hrtimer test start ===\n");
    fails += test_lifecycle();
    fails += test_periodic();
    fails += test_ordering();
    if (!fails) {
        printk_info("hrtimer: all passed\n");
    } else {
        printk_err("hrtimer: %d failures\n", fails);
    }
}

#if KERNEL_CONFIG_ENABLE_TESTS
JLOS_INITCALL(JLOS_INITCALL_TEST, hrtimer_test);
#endif