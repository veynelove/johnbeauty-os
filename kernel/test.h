#ifndef _JLOS_KERNEL_TEST_H
#define _JLOS_KERNEL_TEST_H

#include <common/types.h>

typedef void (*jlos_test_fn_t)(void);

typedef struct {
    const char     *suite;
    const char     *name;
    jlos_test_fn_t  fn;
} jlos_test_case_t;

typedef struct {
    uint32_t asserts;
    uint32_t failures;
} jlos_test_stats_t;

extern jlos_test_stats_t g_jlos_test_stats;

#define JLOS_TEST(suite, name) \
    static void jlos_test_fn_##suite##_##name(void); \
    static const jlos_test_case_t \
        __attribute__((used, section(".jlos_test"))) \
        jlos_test_##suite##_##name = { #suite, #name, jlos_test_fn_##suite##_##name }; \
    static void jlos_test_fn_##suite##_##name(void)

void jlos_test_fail(const char *file, int line, const char *expr);
void jlos_test_fail_eq(const char *file, int line, const char *a_str, const char *e_str,
                       unsigned long a, unsigned long e);
void jlos_test_fail_cmp(const char *file, int line, const char *op,
                        const char *a_str, const char *b_str,
                        unsigned long a, unsigned long b);
void jlos_test_fail_ptr(const char *file, int line, const char *a_str, const char *e_str,
                        const void *a, const void *e);

#define JLOS_TEST_TRUE(cond) \
    do { \
        g_jlos_test_stats.asserts++; \
        if (!(cond)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail(__FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define JLOS_TEST_FALSE(cond) JLOS_TEST_TRUE(!(cond))

#define JLOS_TEST_EQ(actual, expected) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((actual) != (expected)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_eq(__FILE__, __LINE__, #actual, #expected, \
                              (unsigned long)(actual), (unsigned long)(expected)); \
        } \
    } while (0)

#define JLOS_TEST_NE(actual, expected) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((actual) == (expected)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_cmp(__FILE__, __LINE__, "!=", #actual, #expected, \
                               (unsigned long)(actual), (unsigned long)(expected)); \
        } \
    } while (0)

#define JLOS_TEST_GE(a, b) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((a) < (b)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_cmp(__FILE__, __LINE__, ">=", #a, #b, \
                               (unsigned long)(a), (unsigned long)(b)); \
        } \
    } while (0)

#define JLOS_TEST_GT(a, b) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((a) <= (b)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_cmp(__FILE__, __LINE__, ">", #a, #b, \
                               (unsigned long)(a), (unsigned long)(b)); \
        } \
    } while (0)

#define JLOS_TEST_LE(a, b) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((a) > (b)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_cmp(__FILE__, __LINE__, "<=", #a, #b, \
                               (unsigned long)(a), (unsigned long)(b)); \
        } \
    } while (0)

#define JLOS_TEST_LT(a, b) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((a) >= (b)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_cmp(__FILE__, __LINE__, "<", #a, #b, \
                               (unsigned long)(a), (unsigned long)(b)); \
        } \
    } while (0)

#define JLOS_TEST_PTR_EQ(actual, expected) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((const void *)(actual) != (const void *)(expected)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_ptr(__FILE__, __LINE__, #actual, #expected, \
                               (const void *)(actual), (const void *)(expected)); \
        } \
    } while (0)

#define JLOS_TEST_PTR_NE(actual, expected) \
    do { \
        g_jlos_test_stats.asserts++; \
        if ((const void *)(actual) == (const void *)(expected)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail_ptr(__FILE__, __LINE__, #actual, #expected, \
                               (const void *)(actual), (const void *)(expected)); \
        } \
    } while (0)

#define JLOS_TEST_NULL(p)          JLOS_TEST_PTR_EQ(p, NULL)
#define JLOS_TEST_NOT_NULL(p)      JLOS_TEST_TRUE((p) != NULL)

#define JLOS_ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            g_jlos_test_stats.failures++; \
            jlos_test_fail(__FILE__, __LINE__, #cond); \
            return; \
        } \
    } while (0)

#define JLOS_ASSERT_NOT_NULL(p) JLOS_ASSERT_TRUE((p) != NULL)

void jlos_test_run_all(void);

#endif