/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#define JLOS_KERNEL_LOG_SUBSYS "test"
#include <kernel/test.h>
#include <kernel/printk.h>
#include <kernel/initcall.h>
#include <include/config.h>

jlos_test_stats_t g_jlos_test_stats;

extern const jlos_test_case_t __jlos_test_start[];
extern const jlos_test_case_t __jlos_test_end[];

void jlos_test_fail(const char *file, int line, const char *expr)
{
    printk_err("        %s:%d: FAIL %s\n", file, line, expr);
}

void jlos_test_fail_eq(const char *file, int line, const char *a_str, const char *e_str,
                       unsigned long a, unsigned long e)
{
    printk_err("        %s:%d: FAIL %s == %s (0x%lx != 0x%lx)\n",
               file, line, a_str, e_str, a, e);
}

void jlos_test_fail_cmp(const char *file, int line, const char *op,
                        const char *a_str, const char *b_str,
                        unsigned long a, unsigned long b)
{
    printk_err("        %s:%d: FAIL %s %s %s (0x%lx vs 0x%lx)\n",
               file, line, a_str, op, b_str, a, b);
}

void jlos_test_fail_ptr(const char *file, int line, const char *a_str, const char *e_str,
                        const void *a, const void *e)
{
    printk_err("        %s:%d: FAIL %s == %s (%p != %p)\n",
               file, line, a_str, e_str, a, e);
}

#define JLOS_TEST_MAX_SUITES 32

typedef struct {
    const char *name;
    uint32_t    pass;
    uint32_t    fail;
} jlos_test_suite_stat_t;

void jlos_test_run_all(void)
{
    jlos_test_suite_stat_t stats[JLOS_TEST_MAX_SUITES];
    uint32_t num_suites = 0;
    uint32_t total_pass = 0;
    uint32_t total_fail = 0;
    const char *cur_suite = NULL;
    const jlos_test_case_t *t;

    for (t = __jlos_test_start; t < __jlos_test_end; t++) {
        if (cur_suite == NULL || jlos_strcmp(t->suite, cur_suite) != 0) {
            printk_info("=== %s ===\n", t->suite);
            cur_suite = t->suite;
            if (num_suites < JLOS_TEST_MAX_SUITES) {
                stats[num_suites].name = t->suite;
                stats[num_suites].pass = 0;
                stats[num_suites].fail = 0;
                num_suites++;
            }
        }
        g_jlos_test_stats.asserts = 0;
        g_jlos_test_stats.failures = 0;
        t->fn();
        if (g_jlos_test_stats.failures == 0) {
            printk_info("  PASS  %s\n", t->name);
            if (num_suites > 0) {
                stats[num_suites - 1].pass++;
            }
            total_pass++;
        } else {
            printk_err("  FAIL  %s\n", t->name);
            if (num_suites > 0) {
                stats[num_suites - 1].fail++;
            }
            total_fail++;
        }
    }

    printk_info("=== summary ===\n");
    printk_info("%-16s %5s %5s %5s\n", "suite", "pass", "fail", "total");
    for (uint32_t i = 0; i < num_suites; i++) {
        printk_info("%-16s %5u %5u %5u\n",
                    stats[i].name,
                    stats[i].pass,
                    stats[i].fail,
                    stats[i].pass + stats[i].fail);
    }
    printk_info("%-16s %5u %5u %5u\n",
                "total",
                total_pass,
                total_fail,
                total_pass + total_fail);
}

#if KERNEL_CONFIG_ENABLE_TESTS
JLOS_INITCALL(JLOS_INITCALL_TEST, jlos_test_run_all);
#endif
