#include <kernel/tests/multitask_test.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/timek.h>
#include <hal/mmu.h>
#include <hal/hal.h>
#include <hal/context.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_sched"
#include <kernel/printk.h>

static jlos_task_manager_t *s_mgr;

static volatile int g_alt_a = 0, g_alt_b = 0;

static volatile int g_fork_test_exited = 0;
static volatile uint32_t g_fork_test_pid = 0;

static volatile int g_ring3_exited = 0;
static volatile uint32_t g_ring3_pid = 0;

static volatile int g_file_test_exited = 0;
static volatile uint32_t g_file_test_pid = 0;

static volatile int g_signal_test_exited = 0;
static volatile uint32_t g_signal_test_pid = 0;

static volatile int g_mmap_test_exited = 0;
static volatile uint32_t g_mmap_test_pid = 0;

static volatile int g_nanosleep_test_exited = 0;
static volatile uint32_t g_nanosleep_test_pid = 0;

static void alt_task_exit(void)
{
    jlos_task_t *me = jlos_task_manager_curr_task_on_tick(s_mgr);
    if (me)
        jlos_process_exit(me, 0);
}

static void alt_entry_a(void)
{
    for (int i = 0; i < 500; i++) {
        g_alt_a++;
        for (volatile int k = 0; k < 200; k++);
    }
    alt_task_exit();
}

static void alt_entry_b(void)
{
    for (int i = 0; i < 500; i++) {
        g_alt_b++;
        for (volatile int k = 0; k < 200; k++);
    }
    alt_task_exit();
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

static jlos_task_t *spawn_kernel_task(jlos_mmu_t *mmu, void (*fn)(void), const char *name, int *fail_cnt)
{
    jlos_task_t *t = (jlos_task_t *)jlos_kalloc(sizeof(*t));
    if (!t) {
        if (fail_cnt)
            (*fail_cnt)++;
        return NULL;
    }
    if (jlos_task_init(t, mmu, fn, name) < 0) {
        jlos_kfree(t);
        if (fail_cnt)
            (*fail_cnt)++;
        return NULL;
    }
    if (!jlos_task_manager_add_task(s_mgr, t)) {
        jlos_task_free(s_mgr, t);
        if (fail_cnt)
            (*fail_cnt)++;
        return NULL;
    }
    return t;
}

void multitask_test(void)
{
    s_mgr = g_task_manager_ptr;
    jlos_mmu_t *mmu = jlos_mmu_get_kernel();

    printk_info("=== multitask test start ===\n");
    printk_info("MAX_TASKS=%u KSTACK=%uB USTACK=%uKB MLFQ=%u lv\n",
           JLOS_TASK_MAX_NUM, JLOS_TASK_STACK_SIZE,
           JLOS_TASK_USER_STACK_SIZE / 1024, JLOS_TASK_MLFQ_LEVELS);

    g_alt_a = g_alt_b = 0;
    g_fork_test_exited = 0;

    g_ring3_exited = 0;

    int spawn_fails = 0;

    printk_info("[test 1] schedule alternation (2 kernel tasks)\n");
    spawn_kernel_task(mmu, alt_entry_a, "t1_a", &spawn_fails);
    spawn_kernel_task(mmu, alt_entry_b, "t1_b", &spawn_fails);
    
    printk_info("[test 2] fork+wait + 10-child pressure (user ELF)\n");
    {
        jlos_task_t *t = spawn_kernel_task(mmu, fork_test_entry, "t2_fork", &spawn_fails);
        if (t) {
            g_fork_test_pid = t->pid;
        }
    }

    printk_info("[test 4] ring3 user task smoke\n");
    {
        jlos_task_t *t = spawn_kernel_task(mmu, ring3_loader_entry, "t4_ring3", &spawn_fails);
        if (t) {
            g_ring3_pid = t->pid;
        }
    }
    printk_info("[test 5] ring3 file syscall test\n");
    {
        jlos_task_t *t = spawn_kernel_task(mmu, ring3_file_test_entry, "t5_file", &spawn_fails);
        if (t) {
            g_file_test_pid = t->pid;
        }
    }
    printk_info("[test 6] ring3 signal test\n");
    {
        jlos_task_t *t = spawn_kernel_task(mmu, ring3_signal_test_entry, "t6_signal", &spawn_fails);
        if (t) {
            g_signal_test_pid = t->pid;
        }
    }
    printk_info("[test 7] ring3 mmap test\n");
    {
        jlos_task_t *t = spawn_kernel_task(mmu, ring3_mmap_test_entry, "t7_mmap", &spawn_fails);
        if (t) {
            g_mmap_test_pid = t->pid;
        }
    }

    printk_info("[test 8] ring3 nanosleep test\n");
    {
        jlos_task_t *t = spawn_kernel_task(mmu, ring3_nanosleep_test_entry, "t8_nanosleep", &spawn_fails);
        if (t) {
            g_nanosleep_test_pid = t->pid;
        }
    }

    if (spawn_fails) {
        printk_err("spawn stage %d subtask(s) failed, aborting\n", spawn_fails);
        goto teardown;
    }
    printk_info("8 seed tasks spawned, run schedule budget...\n");

    uint32_t t0 = jlos_timek_get_ticks();
    const uint32_t TIMEOUT_TICKS = 5000;
    uint32_t now = t0;
    for (;;) {
        now = jlos_timek_get_ticks();
        if (g_fork_test_pid && !g_fork_test_exited) {
            jlos_task_t *ft2 = jlos_task_manager_find_pid(s_mgr, g_fork_test_pid);
            if (ft2 && ft2->status == JLOS_TASK_ZOMBIE && ft2->exit_code == 42) {
                g_fork_test_exited = 1;
            }
        }
        if (g_ring3_pid && !g_ring3_exited) {
            jlos_task_t *rt = jlos_task_manager_find_pid(s_mgr, g_ring3_pid);
            if (rt && rt->status == JLOS_TASK_ZOMBIE && rt->exit_code == 7) {
                g_ring3_exited = 1;
            }
        }
        if (g_file_test_pid && !g_file_test_exited) {
            jlos_task_t *ft = jlos_task_manager_find_pid(s_mgr, g_file_test_pid);
            if (ft && ft->status == JLOS_TASK_ZOMBIE && ft->exit_code == 42) {
                g_file_test_exited = 1;
            }
        }
        if (g_signal_test_pid && !g_signal_test_exited) {
            jlos_task_t *st = jlos_task_manager_find_pid(s_mgr, g_signal_test_pid);
            if (st && st->status == JLOS_TASK_ZOMBIE && st->exit_code == 42) {
                g_signal_test_exited = 1;
            }
        }
        if (g_mmap_test_pid && !g_mmap_test_exited) {
            jlos_task_t *mt = jlos_task_manager_find_pid(s_mgr, g_mmap_test_pid);
            if (mt && mt->status == JLOS_TASK_ZOMBIE && mt->exit_code == 42) {
                g_mmap_test_exited = 1;
            }
        }
        if (g_nanosleep_test_pid && !g_nanosleep_test_exited) {
            jlos_task_t *nt = jlos_task_manager_find_pid(s_mgr, g_nanosleep_test_pid);
            if (nt && nt->status == JLOS_TASK_ZOMBIE && nt->exit_code == 42) {
                g_nanosleep_test_exited = 1;
            }
        }
        int all = (g_alt_a >= 500 && g_alt_b >= 500) && (g_fork_test_exited >= 1)
            && (g_ring3_exited >= 1) && (g_file_test_exited >= 1) && (g_signal_test_exited >= 1)
            && (g_mmap_test_exited >= 1) && (g_nanosleep_test_exited >= 1);
        if (all)
            break;
        if (now - t0 >= TIMEOUT_TICKS)
            break;
        for (volatile int k = 0; k < 20000; k++);
    }
    printk_info("budget: elapsed=%u ticks (start=%u now=%u)\n",
           (unsigned)(now - t0), (unsigned)t0, (unsigned)now);

    int fails = 0;
    printk_info("subcase results:\n");

    if (g_alt_a >= 500 && g_alt_b >= 500)
        printk_info("[1] alternation: A=%d B=%d\n", g_alt_a, g_alt_b);
    else {
        printk_err("[1] alternation: A=%d B=%d (both >= 500)\n", g_alt_a, g_alt_b);
        fails++;
    }

    if (g_fork_test_exited >= 1)
        printk_info("[2] fork+pressure: exited=%d\n", g_fork_test_exited);
    else {
        printk_err("[2] fork+pressure: exited=%d\n", g_fork_test_exited);
        fails++;
    }

    if (g_ring3_exited >= 1)
        printk_info("[4] ring3 smoke: exited=%d\n", g_ring3_exited);
    else {
        printk_err("[4] ring3 smoke: exited=%d\n", g_ring3_exited);
        fails++;
    }

    if (g_file_test_exited >= 1)
        printk_info("[5] file syscall: exited=%d\n", g_file_test_exited);
    else {
        printk_err("[5] file syscall: exited=%d\n", g_file_test_exited);
        fails++;
    }

    if (g_signal_test_exited >= 1)
        printk_info("[6] signal: exited=%d\n", g_signal_test_exited);
    else {
        printk_err("[6] signal: exited=%d\n", g_signal_test_exited);
        fails++;
    }

    if (g_mmap_test_exited >= 1)
        printk_info("[7] mmap: exited=%d\n", g_mmap_test_exited);
    else {
        printk_err("[7] mmap: exited=%d\n", g_mmap_test_exited);
        fails++;
    }

    if (g_nanosleep_test_exited >= 1)
        printk_info("[8] nanosleep: exited=%d\n", g_nanosleep_test_exited);
    else {
        printk_err("[8] nanosleep: exited=%d\n", g_nanosleep_test_exited);
        fails++;
    }

    if (!fails)
        printk_info("multitask: all passed\n");
    else
        printk_err("multitask: %d failures\n", fails);

teardown:
    ;
    int i = 0;
    while ((i = s_mgr->num_tasks - 1) > 0) {
        jlos_task_t *t = s_mgr->tasks[i];
        if (!t || t == s_mgr->idle_task)
            continue;
        if (!jlos_list_empty(&t->zombie_node))
            jlos_list_del_init(&t->zombie_node);
        jlos_task_free(s_mgr, t);
    }
}
#if KERNEL_CONFIG_ENABLE_TESTS
JLOS_INITCALL(JLOS_INITCALL_TEST, multitask_test);
#endif
