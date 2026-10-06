#include <kernel/test.h>
#include <kernel/hrtimer.h>
#include <kernel/timek.h>
#include <dsa/rbtree.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_hrtm"
#include <kernel/printk.h>

static int test_callback(jlos_hrtimer_t *t)
{
    (void)t;
    return 0;
}

JLOS_TEST(hrtimer, lifecycle)
{
    jlos_hrtimer_base_t base;
    jlos_hrtimer_base_init(&base);
    jlos_hrtimer_t timer;
    jlos_hrtimer_init(&timer, test_callback, NULL);
    JLOS_TEST_FALSE(timer.active);
    JLOS_TEST_EQ(timer.expires, 0);
    jlos_hrtimer_start(&base, &timer, 1000000, 0, JLOS_HRTIMER_MODE_REL);
    JLOS_TEST_TRUE(timer.active);
    JLOS_TEST_EQ(timer.interval, 0);
    JLOS_TEST_NE(timer.expires, 0);
    jlos_hrtimer_cancel(&base, &timer);
    JLOS_TEST_FALSE(timer.active);
}

JLOS_TEST(hrtimer, periodic)
{
    jlos_hrtimer_base_t base;
    jlos_hrtimer_base_init(&base);
    jlos_hrtimer_t timer;
    jlos_hrtimer_init(&timer, test_callback, NULL);
    jlos_hrtimer_start(&base, &timer, 1000000, 500000, JLOS_HRTIMER_MODE_REL);
    JLOS_TEST_EQ(timer.interval, 500000);
    jlos_hrtimer_cancel(&base, &timer);
}

JLOS_TEST(hrtimer, ordering)
{
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
    JLOS_TEST_NOT_NULL(first);
    if (first) {
        jlos_hrtimer_t *earliest = jlos_rbtree_entry(first, jlos_hrtimer_t, node);
        JLOS_TEST_PTR_EQ(earliest, &t2);
    }
    jlos_hrtimer_cancel(&base, &t1);
    jlos_hrtimer_cancel(&base, &t2);
    jlos_hrtimer_cancel(&base, &t3);
}