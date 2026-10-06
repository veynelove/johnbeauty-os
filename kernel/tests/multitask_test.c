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
        return NULL;
    }
    if (jlos_task_init(t, mmu, fn, name) < 0) {
        jlos_kfree(t);
        return NULL;
    }
    if (!jlos_task_manager_add_task(s_mgr, t)) {
        jlos_task_free(s_mgr, t);
        return NULL;
    }
    return t;
}

static void reap_zombies(void)
{
    for (int i = s_mgr->num_tasks - 1; i > 0; i--) {
        jlos_task_t *t = s_mgr->tasks[i];
        if (!t || t == s_mgr->idle_task) {
            continue;
        }
        if (t->status == JLOS_TASK_ZOMBIE) {
            if (!jlos_list_empty(&t->zombie_node)) {
                jlos_list_del_init(&t->zombie_node);
            }
            jlos_task_free(s_mgr, t);
        }
    }
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
    char *argv[] = {"/fork_test.elf", NULL};
    char *envp[] = {NULL};
    int ret = jlos_process_exec_elf(g_current_task_ptr, "/fork_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(g_current_task_ptr, 0);
    }
}

static void ring3_loader_entry(void)
{
    char *argv[] = {"/hello.elf", NULL};
    char *envp[] = {NULL};
    int ret = jlos_process_exec_elf(g_current_task_ptr, "/hello.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(g_current_task_ptr, 0);
    }
}

static void ring3_file_test_entry(void)
{
    char *argv[] = {"/file_test.elf", NULL};
    char *envp[] = {NULL};
    int ret = jlos_process_exec_elf(g_current_task_ptr, "/file_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(g_current_task_ptr, 0);
    }
}

static void ring3_signal_test_entry(void)
{
    char *argv[] = {"/signal_test.elf", NULL};
    char *envp[] = {NULL};
    int ret = jlos_process_exec_elf(g_current_task_ptr, "/signal_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(g_current_task_ptr, 0);
    }
}

static void ring3_mmap_test_entry(void)
{
    char *argv[] = {"/mmap_test.elf", NULL};
    char *envp[] = {NULL};
    int ret = jlos_process_exec_elf(g_current_task_ptr, "/mmap_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(g_current_task_ptr, 0);
    }
}

static void ring3_nanosleep_test_entry(void)
{
    char *argv[] = {"/nanosleep_test.elf", NULL};
    char *envp[] = {NULL};
    int ret = jlos_process_exec_elf(g_current_task_ptr, "/nanosleep_test.elf", 1, argv, envp);
    if (ret < 0) {
        jlos_process_exit(g_current_task_ptr, 0);
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
        jlos_task_t *ft = jlos_task_manager_find_pid(s_mgr, pid);
        if (ft && ft->status == JLOS_TASK_ZOMBIE && ft->exit_code == 42) {
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
        jlos_task_t *rt = jlos_task_manager_find_pid(s_mgr, pid);
        if (rt && rt->status == JLOS_TASK_ZOMBIE && rt->exit_code == 7) {
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
        jlos_task_t *ft = jlos_task_manager_find_pid(s_mgr, pid);
        if (ft && ft->status == JLOS_TASK_ZOMBIE && ft->exit_code == 42) {
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
        jlos_task_t *st = jlos_task_manager_find_pid(s_mgr, pid);
        if (st && st->status == JLOS_TASK_ZOMBIE && st->exit_code == 42) {
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
        jlos_task_t *mt = jlos_task_manager_find_pid(s_mgr, pid);
        if (mt && mt->status == JLOS_TASK_ZOMBIE && mt->exit_code == 42) {
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
        jlos_task_t *nt = jlos_task_manager_find_pid(s_mgr, pid);
        if (nt && nt->status == JLOS_TASK_ZOMBIE && nt->exit_code == 42) {
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
