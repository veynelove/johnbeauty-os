/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <kernel/timek.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_timek"
#include <kernel/printk.h>

JLOS_TEST(timek, monotonic_increasing)
{
    uint64_t t1 = jlos_timek_get_monotonic_ns();
    uint64_t t2 = jlos_timek_get_monotonic_ns();
    JLOS_TEST_GT(t2, t1);
}

JLOS_TEST(timek, ticks_non_decreasing)
{
    uint32_t t1 = jlos_timek_get_ticks();
    for (volatile int i = 0; i < 100000; i++) {
    }
    uint32_t t2 = jlos_timek_get_ticks();
    JLOS_TEST_GE(t2, t1);
}

JLOS_TEST(timek, realtime_ge_monotonic)
{
    uint64_t mono = jlos_timek_get_monotonic_ns();
    uint64_t rt = jlos_timek_get_realtime_ns();
    JLOS_TEST_GE(rt, mono);
}