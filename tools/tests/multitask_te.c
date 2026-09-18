#include <tools/tests/multitask_te.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <hal/hal.h>
#include <hal/context.h>
#include <hal/timer.h>
#include <lib/user_syscall.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"

static jlos_task_manager_t *s_mgr;

static volatile int g_alt_a = 0, g_alt_b = 0;

#define FORK_MAGIC_EXIT 42u
static volatile uint32_t g_fork_child_pid = 0;
static volatile int g_fork_done = 0;
static volatile int g_fork_parent_done = 0;
static volatile uint32_t g_fork_exit_code = 0;

#define PRESSURE_CHILDREN 10
static volatile uint32_t g_pressure_codes[PRESSURE_CHILDREN];
static volatile uint32_t g_pressure_child_pids[PRESSURE_CHILDREN];
static volatile int g_pressure_done = 0;
static volatile int g_pressure_fork_done = 0;


static volatile int g_ring3_exited = 0;

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

/* omit-frame-pointer: 子栈是父栈 memcpy, 必须 esp 相对寻址访问子栈局部变量 */
__attribute__((optimize("omit-frame-pointer")))
static void fork_parent_entry(void)
{
    jlos_task_t *me = jlos_task_curr();
    if (!me) {
        g_fork_parent_done = -1;
        return;
    }

    void *child = jlos_arch_fork_invoke(s_mgr, me);
    me = jlos_task_curr();
    if (!child) {
        jlos_process_exit(me, FORK_MAGIC_EXIT);
        return;
    }

    g_fork_child_pid = ((jlos_task_t *)child)->pid;
    g_fork_done = 1;
    jlos_task_set_waiting(me, g_fork_child_pid);
    jlos_task_manager_schedule(s_mgr);
    jlos_task_t *z = jlos_task_manager_find_pid(s_mgr, g_fork_child_pid);
    if (!z || z->status != JLOS_TASK_ZOMBIE) {
        g_fork_parent_done = -3;
        return;
    }
    g_fork_exit_code = z->exit_code;
    jlos_task_free(s_mgr, z);
    g_fork_parent_done = 1;
    jlos_process_exit(me, 0);
}

/* omit-frame-pointer: 子栈是父栈 memcpy, 必须 esp 相对寻址访问子栈局部变量 */
__attribute__((optimize("omit-frame-pointer")))
static void pressure_parent_entry(void)
{
    jlos_task_t *me = jlos_task_curr();
    if (!me) {
        g_pressure_done = -1;
        return;
    }

    void *c = jlos_arch_fork_invoke(s_mgr, me);
    me = jlos_task_curr();

    if (!c) {
        while (!g_pressure_fork_done);
        me = jlos_task_curr();
        uint32_t my_pid = me ? (uint32_t)me->pid : 0u;
        for (int i = 0; i < PRESSURE_CHILDREN; i++) {
            if (g_pressure_child_pids[i] == my_pid) {
                jlos_process_exit(me, 100u + (uint32_t)i);
                return;
            }
        }
        jlos_process_exit(me, 0xDEAD);
        return;
    }

    g_pressure_child_pids[0] = ((jlos_task_t *)c)->pid;

    for (int i = 1; i < PRESSURE_CHILDREN; i++) {
        void *c2 = jlos_arch_fork_invoke(s_mgr, me);
        me = jlos_task_curr();
        if (!c2) {
            while (!g_pressure_fork_done);
            me = jlos_task_curr();
            uint32_t my_pid = me ? (uint32_t)me->pid : 0u;
            for (int j = 0; j < PRESSURE_CHILDREN; j++) {
                if (g_pressure_child_pids[j] == my_pid) {
                    jlos_process_exit(me, 100u + (uint32_t)j);
                    return;
                }
            }
            jlos_process_exit(me, 0xDEAD);
            return;
        }
        g_pressure_child_pids[i] = ((jlos_task_t *)c2)->pid;

    }
    g_pressure_fork_done = 1;

    for (int i = 0; i < PRESSURE_CHILDREN; i++) {

        jlos_task_set_waiting(me, g_pressure_child_pids[i]);
        jlos_task_manager_schedule(s_mgr);
        jlos_task_t *cc = jlos_task_manager_find_pid(s_mgr, g_pressure_child_pids[i]);
        if (!cc || cc->status != JLOS_TASK_ZOMBIE) {
            g_pressure_done = -2;
            jlos_process_exit(me, 0);
        }
        g_pressure_codes[i] = (uint32_t)cc->exit_code;
        jlos_task_free(s_mgr, cc);
    }
    g_pressure_done = 1;
    jlos_process_exit(me, 0);
}

static void ring3_entry(void)
{
    uint32_t pid = jlos_user_get_pid();
    jlos_user_printf("ring3: PID=%u\n", pid);
    jlos_user_get_tasks_info();
    jlos_user_sleep(50);
    jlos_user_puts("ring3: wakeup -> exit\n");
    g_ring3_exited++;
    jlos_user_exit(7);
}

static jlos_task_t *spawn_user_task(jlos_mmu_t *mmu, void (*fn)(void), const char *name, int *fail_cnt)
{
    jlos_task_t *t = (jlos_task_t *)jlos_kalloc(sizeof(*t));
    if (!t) {
        if (fail_cnt)
            (*fail_cnt)++;
        return NULL;
    }
    if (jlos_task_init_user(t, mmu, fn, name) < 0) {
        jlos_kfree(t);
        if (fail_cnt)
            (*fail_cnt)++;
        return NULL;
    }
    if (!jlos_task_manager_add_task(s_mgr, t)) {
        jlos_task_free(s_mgr, t);
        jlos_kfree(t);
        if (fail_cnt)
            (*fail_cnt)++;
        return NULL;
    }
    return t;
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
        jlos_kfree(t);
        if (fail_cnt)
            (*fail_cnt)++;
        return NULL;
    }
    return t;
}

void multitask_test(jlos_mmu_t *mmu, jlos_task_manager_t *task_manager_)
{
    s_mgr = task_manager_;

    printk_info("=== multitask test start ===\n");
    printk_info("MAX_TASKS=%u KSTACK=%uB USTACK=%uKB MLFQ=%u lv\n",
           JLOS_TASK_MAX_NUM, JLOS_TASK_STACK_SIZE,
           JLOS_TASK_USER_STACK_SIZE / 1024, JLOS_TASK_MLFQ_LEVELS);

    g_alt_a = g_alt_b = 0;
    g_fork_child_pid = g_fork_exit_code = g_fork_done = 0;
    g_fork_parent_done = 0;
    for (int i = 0; i < PRESSURE_CHILDREN; i++)
        g_pressure_codes[i] = 0;
    for (int i = 0; i < PRESSURE_CHILDREN; i++)
        g_pressure_child_pids[i] = 0;
    g_pressure_done = g_pressure_fork_done = 0;

    g_ring3_exited = 0;

    int spawn_fails = 0;

    printk_info("[test 1] schedule alternation (2 kernel tasks)\n");
    spawn_kernel_task(mmu, alt_entry_a, "t1_a", &spawn_fails);
    spawn_kernel_task(mmu, alt_entry_b, "t1_b", &spawn_fails);
    printk_info("[test 2] fork -> wait -> exit_code (1 child, magic=%u)\n", FORK_MAGIC_EXIT);
    spawn_kernel_task(mmu, fork_parent_entry, "t2_parent", &spawn_fails);
    printk_info("[test 3] fork %u children (exit_code = 100..%u)\n",
           PRESSURE_CHILDREN, 100u + PRESSURE_CHILDREN - 1);
    spawn_kernel_task(mmu, pressure_parent_entry, "t3_parent", &spawn_fails);
    printk_info("[test 4] ring3 user task smoke\n");
    spawn_user_task(mmu, ring3_entry, "t4_ring3", &spawn_fails);

    if (spawn_fails) {
        printk_err("FAIL: spawn stage %d subtask(s) failed, aborting\n", spawn_fails);
        goto teardown;
    }
    printk_info("OK: 5 seed tasks spawned, run schedule budget...\n");

    uint32_t t0 = jlos_hal_timer_get_ticks();
    const uint32_t TIMEOUT_TICKS = 5000;
    uint32_t now = t0;
    for (;;) {
        now = jlos_hal_timer_get_ticks();
        int all = (g_alt_a >= 500 && g_alt_b >= 500)
               && (g_fork_parent_done == 1)
               && (g_pressure_done == 1)
               && (g_ring3_exited >= 1);
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

    printk_info("[1] alternation: A=%d B=%d -> ", g_alt_a, g_alt_b);
    if (g_alt_a >= 500 && g_alt_b >= 500)
        printk_info("PASS\n");
    else {
        printk_err("FAIL (both >= 500)\n");
        fails++;
    }

    printk_info("[2] fork-wait: status=%d pid=%u exit=%u -> ",
           g_fork_parent_done, (unsigned)g_fork_child_pid, (unsigned)g_fork_exit_code);
    if (g_fork_parent_done == 1 && g_fork_child_pid > 0 && g_fork_exit_code == FORK_MAGIC_EXIT)
        printk_info("PASS\n");
    else {
        printk_err("FAIL (want status=1, pid>0, exit=%u)\n", FORK_MAGIC_EXIT);
        fails++;
    }

    printk_info("[3] 10-child pressure: status=%d\n", g_pressure_done);
    if (g_pressure_done == 1) {
        int bad = 0;
        for (int i = 0; i < PRESSURE_CHILDREN; i++) {
            if (g_pressure_codes[i] != 100u + (uint32_t)i) {
                printk_err("mismatch i=%u exit=%u expect=%u\n",
                       i, (unsigned)g_pressure_codes[i], 100u + i);
                bad++;
            }
        }
        if (!bad)
            printk_info("all 10 exit_codes matched, PASS\n");
        else {
            printk_err("%u mismatch, FAIL\n", bad);
            fails++;
        }
    } else {
        printk_err("FAIL status=%d\n", g_pressure_done);
        fails++;
    }

    printk_info("[4] ring3 smoke: exited=%d -> ", g_ring3_exited);
    if (g_ring3_exited >= 1)
        printk_info("PASS\n");
    else {
        printk_err("FAIL\n");
        fails++;
    }

    if (!fails)
        printk_info("multitask: ALL PASSED\n");
    else
        printk_err("multitask: FAILS: %d, check above\n", fails);

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