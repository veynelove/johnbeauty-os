/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/test.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/timek.h>
#include <hal/mmu.h>
#include <hal/hal.h>
#include <hal/context.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_sched"
#include <kernel/printk.h>

static jlos_task_manager_t *s_mgr;

static jlos_task_t *spawn_kernel_task(void (*fn)(void), const char *name)
{
    jlos_mmu_t *mmu = jlos_mmu_get_kernel();
    jlos_task_t *t = (jlos_task_t *)jlos_kalloc(sizeof(*t));
    if (!t) {
        return nullptr;
    }
    if (jlos_task_init(t, mmu, fn, name) < 0) {
        jlos_kfree(t);
        return nullptr;
    }
    if (!jlos_task_manager_add_task(s_mgr, t)) {
        jlos_task_free(s_mgr, t);
        return nullptr;
    }
    jlos_task_manager_enqueue(s_mgr, t);
    return t;
}

static void reap_zombies(void)
{
    jlos_task_reap_all_zombies(s_mgr);
}

static void kernel_task_exit(void)
{
    jlos_task_t *me = jlos_task_manager_curr_task_on_tick(s_mgr);
    if (me) {
        jlos_process_exit(me, 0);
    }
}

static volatile int g_alt_a = 0, g_alt_b = 0;

static void alt_entry_a(void)
{
    for (int i = 0; i < 500; i++) {
        g_alt_a++;
        for (volatile int k = 0; k < 200; k++);
    }
    kernel_task_exit();
}

static void alt_entry_b(void)
{
    for (int i = 0; i < 500; i++) {
        g_alt_b++;
        for (volatile int k = 0; k < 200; k++);
    }
    kernel_task_exit();
}

static void fork_test_entry(void)
{
    char *argv[] = {"/fork_test.elf", nullptr};
    char *envp[] = {nullptr};
    jlos_task_t *current_task = jlos_hal_current_task();
    int ret = jlos_process_exec_elf(current_task, "/fork_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(current_task, 0);
    }
}

static void ring3_loader_entry(void)
{
    char *argv[] = {"/hello.elf", nullptr};
    char *envp[] = {nullptr};
    jlos_task_t *current_task = jlos_hal_current_task();
    int ret = jlos_process_exec_elf(current_task, "/hello.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(current_task, 0);
    }
}

static void ring3_file_test_entry(void)
{
    char *argv[] = {"/file_test.elf", nullptr};
    char *envp[] = {nullptr};
    jlos_task_t *current_task = jlos_hal_current_task();
    int ret = jlos_process_exec_elf(current_task, "/file_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(current_task, 0);
    }
}

static void ring3_signal_test_entry(void)
{
    char *argv[] = {"/signal_test.elf", nullptr};
    char *envp[] = {nullptr};
    jlos_task_t *current_task = jlos_hal_current_task();
    int ret = jlos_process_exec_elf(current_task, "/signal_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(current_task, 0);
    }
}

static void ring3_mmap_test_entry(void)
{
    char *argv[] = {"/mmap_test.elf", nullptr};
    char *envp[] = {nullptr};
    jlos_task_t *current_task = jlos_hal_current_task();
    int ret = jlos_process_exec_elf(current_task, "/mmap_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(current_task, 0);
    }
}

static void ring3_nanosleep_test_entry(void)
{
    char *argv[] = {"/nanosleep_test.elf", nullptr};
    char *envp[] = {nullptr};
    jlos_task_t *current_task = jlos_hal_current_task();
    int ret = jlos_process_exec_elf(current_task, "/nanosleep_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(current_task, 0);
    }
}

static volatile int g_sleep_short_done = 0;
static volatile uint32_t g_sleep_short_wake_tick = 0;

static void sleep_short_entry(void)
{
    uint32_t target = jlos_timek_get_ticks() + 50;
    jlos_task_sleep_until(g_task_manager_ptr, target);
    g_sleep_short_wake_tick = jlos_timek_get_ticks();
    g_sleep_short_done = 1;
    kernel_task_exit();
}

static volatile int g_sleep_cascade_done = 0;
static volatile uint32_t g_sleep_cascade_wake_tick = 0;

static void sleep_cascade_entry(void)
{
    uint32_t target = jlos_timek_get_ticks() + 300;
    jlos_task_sleep_until(g_task_manager_ptr, target);
    g_sleep_cascade_wake_tick = jlos_timek_get_ticks();
    g_sleep_cascade_done = 1;
    kernel_task_exit();
}

JLOS_TEST(multitask, schedule_alternation)
{
    s_mgr = g_task_manager_ptr;
    g_alt_a = 0;
    g_alt_b = 0;
    JLOS_ASSERT_NOT_NULL(spawn_kernel_task(alt_entry_a, "mt_alt_a"));
    JLOS_ASSERT_NOT_NULL(spawn_kernel_task(alt_entry_b, "mt_alt_b"));

    uint32_t t0 = jlos_timek_get_ticks();
    while (!(g_alt_a >= 500 && g_alt_b >= 500) && jlos_timek_get_ticks() - t0 < 5000) {
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_GE(g_alt_a, 500);
    JLOS_TEST_GE(g_alt_b, 500);
    reap_zombies();
}

JLOS_TEST(multitask, fork_pressure)
{
    s_mgr = g_task_manager_ptr;
    jlos_task_t *t = spawn_kernel_task(fork_test_entry, "mt_fork");
    JLOS_ASSERT_NOT_NULL(t);
    uint32_t pid = t->pid;
    int exited = 0;

    uint32_t t0 = jlos_timek_get_ticks();
    while (!exited && jlos_timek_get_ticks() - t0 < 5000) {
        if (jlos_task_check_zombie_exit(s_mgr, pid, 42)) {
            exited = 1;
        }
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(exited);
    reap_zombies();
}

JLOS_TEST(multitask, ring3_smoke)
{
    s_mgr = g_task_manager_ptr;
    jlos_task_t *t = spawn_kernel_task(ring3_loader_entry, "mt_ring3");
    JLOS_ASSERT_NOT_NULL(t);
    uint32_t pid = t->pid;
    int exited = 0;

    uint32_t t0 = jlos_timek_get_ticks();
    while (!exited && jlos_timek_get_ticks() - t0 < 5000) {
        if (jlos_task_check_zombie_exit(s_mgr, pid, 7)) {
            exited = 1;
        }
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(exited);
    reap_zombies();
}

JLOS_TEST(multitask, ring3_file)
{
    s_mgr = g_task_manager_ptr;
    jlos_task_t *t = spawn_kernel_task(ring3_file_test_entry, "mt_file");
    JLOS_ASSERT_NOT_NULL(t);
    uint32_t pid = t->pid;
    int exited = 0;

    uint32_t t0 = jlos_timek_get_ticks();
    while (!exited && jlos_timek_get_ticks() - t0 < 5000) {
        if (jlos_task_check_zombie_exit(s_mgr, pid, 42)) {
            exited = 1;
        }
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(exited);
    reap_zombies();
}

JLOS_TEST(multitask, ring3_signal)
{
    s_mgr = g_task_manager_ptr;
    jlos_task_t *t = spawn_kernel_task(ring3_signal_test_entry, "mt_signal");
    JLOS_ASSERT_NOT_NULL(t);
    uint32_t pid = t->pid;
    int exited = 0;

    uint32_t t0 = jlos_timek_get_ticks();
    while (!exited && jlos_timek_get_ticks() - t0 < 5000) {
        if (jlos_task_check_zombie_exit(s_mgr, pid, 42)) {
            exited = 1;
        }
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(exited);
    reap_zombies();
}

JLOS_TEST(multitask, ring3_mmap)
{
    s_mgr = g_task_manager_ptr;
    jlos_task_t *t = spawn_kernel_task(ring3_mmap_test_entry, "mt_mmap");
    JLOS_ASSERT_NOT_NULL(t);
    uint32_t pid = t->pid;
    int exited = 0;

    uint32_t t0 = jlos_timek_get_ticks();
    while (!exited && jlos_timek_get_ticks() - t0 < 5000) {
        if (jlos_task_check_zombie_exit(s_mgr, pid, 42)) {
            exited = 1;
        }
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(exited);
    reap_zombies();
}

JLOS_TEST(multitask, ring3_nanosleep)
{
    s_mgr = g_task_manager_ptr;
    jlos_task_t *t = spawn_kernel_task(ring3_nanosleep_test_entry, "mt_nanosleep");
    JLOS_ASSERT_NOT_NULL(t);
    uint32_t pid = t->pid;
    int exited = 0;

    uint32_t t0 = jlos_timek_get_ticks();
    while (!exited && jlos_timek_get_ticks() - t0 < 5000) {
        if (jlos_task_check_zombie_exit(s_mgr, pid, 42)) {
            exited = 1;
        }
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(exited);
    reap_zombies();
}

JLOS_TEST(multitask, sleep_until_short)
{
    s_mgr = g_task_manager_ptr;
    g_sleep_short_done = 0;
    uint32_t t0 = jlos_timek_get_ticks();
    JLOS_ASSERT_NOT_NULL(spawn_kernel_task(sleep_short_entry, "mt_sleep_s"));

    while (!g_sleep_short_done && jlos_timek_get_ticks() - t0 < 500) {
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(g_sleep_short_done);
    if (g_sleep_short_done) {
        JLOS_TEST_GE(g_sleep_short_wake_tick, t0 + 50);
    }
    reap_zombies();
}

JLOS_TEST(multitask, sleep_until_cascade)
{
    s_mgr = g_task_manager_ptr;
    g_sleep_cascade_done = 0;
    uint32_t t0 = jlos_timek_get_ticks();
    JLOS_ASSERT_NOT_NULL(spawn_kernel_task(sleep_cascade_entry, "mt_sleep_c"));

    while (!g_sleep_cascade_done && jlos_timek_get_ticks() - t0 < 1000) {
        for (volatile int k = 0; k < 20000; k++);
    }
    JLOS_TEST_TRUE(g_sleep_cascade_done);
    if (g_sleep_cascade_done) {
        JLOS_TEST_GE(g_sleep_cascade_wake_tick, t0 + 300);
    }
    reap_zombies();
}
