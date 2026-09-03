#include <tools/tests/multitask_te.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <hal/hal.h>
#include <hal/context.h>
#include <hal/timer.h>
#include <hal/user_syscall.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"

static jlos_task_manager_t *s_mgr;

static volatile int v_t1_a = 0, v_t1_b = 0;

static volatile uint32_t v_t2_child_pid   = 0;
static volatile int      v_t2_fork_done   = 0;
static volatile uint32_t v_t2_exit_code   = 0;
static volatile int      v_t2_parent_done = 0;
#define T2_MAGIC_EXIT  42u

#define T3_CHILDREN  10
static volatile uint32_t v_t3_codes[T3_CHILDREN];
static volatile uint32_t v_t3_child_pids[T3_CHILDREN];  /* 全局 pid 表: fork 克隆栈, 栈上表子读不到父后续写入 */
static volatile int      v_t3_done = 0;
static volatile int      v_t3_fork_done = 0;

static volatile int v_t4_ring3_exited = 0;

/* ---- TEST 1: 调度交替 ---- */
static inline void t1_exit(void)
{
    jlos_task_t *me = jlos_task_manager_curr_task_on_tick(s_mgr);
    if (me) jlos_process_exit(me, 0);
}

static void test1_entry_a(void)
{
    for (int i = 0; i < 500; i++) {
        v_t1_a++;
        for (volatile int k = 0; k < 200; k++);
    }
    t1_exit();
}

static void test1_entry_b(void)
{
    for (int i = 0; i < 500; i++) {
        v_t1_b++;
        for (volatile int k = 0; k < 200; k++);
    }
    t1_exit();
}

/* omit-frame-pointer: 子栈是父栈 memcpy, ebp=父值指向父 kstack, 必须用 esp
 * 相对寻址访问子栈局部变量, 否则父子同读父 kstack 造成混乱. */
__attribute__((optimize("omit-frame-pointer")))
static void test2_parent_entry(void)
{
    jlos_task_t *me = jlos_task_curr();
    if (!me) { v_t2_parent_done = -1; return; }

    void *child = jlos_arch_fork_invoke(s_mgr, me);
    me = jlos_task_curr();          /* 子栈 memcpy 父值, 必须重读全局 */
    if (!child) { jlos_process_exit(me, T2_MAGIC_EXIT); return; }

    v_t2_child_pid = ((jlos_task_t *)child)->pid;
    v_t2_fork_done = 1;
    jlos_task_set_waiting(me, v_t2_child_pid);
    for (volatile long spin = 0;
         me->status == JLOS_TASK_WAITING && spin < 500000000L; spin++);
    jlos_task_t *z = jlos_task_manager_find_pid(s_mgr, v_t2_child_pid);
    if (!z || z->status != JLOS_TASK_ZOMBIE) { v_t2_parent_done = -3; return; }
    v_t2_exit_code = z->exit_code;
    jlos_task_free(s_mgr, z);
    v_t2_parent_done = 1;
    jlos_process_exit(me, 0);
}

__attribute__((optimize("omit-frame-pointer")))
static void test3_parent_entry(void)
{
    jlos_task_t *me = jlos_task_curr();
    if (!me) { v_t3_done = -1; return; }

    void *c = jlos_arch_fork_invoke(s_mgr, me);
    me = jlos_task_curr();

    if (!c) {
        /* 子: 等父填完全局 pid 表后匹配自己 exit */
        while (!v_t3_fork_done);
        me = jlos_task_curr();
        uint32_t my_pid = me ? (uint32_t)me->pid : 0u;
        for (int i = 0; i < T3_CHILDREN; i++) {
            if (v_t3_child_pids[i] == my_pid) {
                jlos_process_exit(me, 100u + (uint32_t)i);
                return;
            }
        }
        jlos_process_exit(me, 0xDEAD);
        return;
    }

    /* 父: fork 其余 9 个, 填全局 pid 表 */
    v_t3_child_pids[0] = ((jlos_task_t *)c)->pid;
    for (int i = 1; i < T3_CHILDREN; i++) {
        void *c2 = jlos_arch_fork_invoke(s_mgr, me);
        me = jlos_task_curr();
        if (!c2) {
            /* 子 (第 i 次 fork): 等父填 pid 表后匹配 exit */
            while (!v_t3_fork_done);
            me = jlos_task_curr();
            uint32_t my_pid = me ? (uint32_t)me->pid : 0u;
            for (int j = 0; j < T3_CHILDREN; j++) {
                if (v_t3_child_pids[j] == my_pid) {
                    jlos_process_exit(me, 100u + (uint32_t)j);
                    return;
                }
            }
            jlos_process_exit(me, 0xDEAD);
            return;
        }
        v_t3_child_pids[i] = ((jlos_task_t *)c2)->pid;
    }
    v_t3_fork_done = 1;

    /* 逐子 set_waiting + 回收: 父主动 WAITING 让出 CPU, 调度器在子 ZOMBIE
     * 时自动唤醒父 (经典 wait/wake 阻塞语义, 替代原 busy-wait budget loop). */
    for (int i = 0; i < T3_CHILDREN; i++) {
        jlos_task_set_waiting(me, v_t3_child_pids[i]);
        for (volatile long spin = 0;
             me->status == JLOS_TASK_WAITING && spin < 500000000L; spin++);
        jlos_task_t *cc = jlos_task_manager_find_pid(s_mgr, v_t3_child_pids[i]);
        if (!cc || cc->status != JLOS_TASK_ZOMBIE) {
            v_t3_done = -2;
            jlos_process_exit(me, 0);
        }
        v_t3_codes[i] = (uint32_t)cc->exit_code;
        jlos_task_free(s_mgr, cc);
    }
    v_t3_done = 1;
    jlos_process_exit(me, 0);
}

/* ---- TEST 4: ring3 冒烟 ---- */
static void test4_ring3_entry(void)
{
    uint32_t pid = jlos_user_get_pid();
    jlos_user_printf("ring3: PID=%u\n", pid);
    jlos_user_get_tasks_info();
    jlos_user_sleep(50);
    jlos_user_puts("ring3: wakeup -> exit\n");
    v_t4_ring3_exited++;
    jlos_user_exit(7);
}

/* ---- 公共 helper: kalloc + task_init + add_task (每步判错 + 对称清理) ---- */
static jlos_task_t *spawn_user_task(jlos_mmu_t *mmu,
                                    void (*fn)(void),
                                    const char *name,
                                    int *fail_cnt)
{
    jlos_task_t *t = (jlos_task_t *)jlos_kalloc(sizeof(*t));
    if (!t) { if (fail_cnt) (*fail_cnt)++; return NULL; }
    if (jlos_task_init_user(t, mmu, fn, name) < 0) {
        jlos_kfree(t);
        if (fail_cnt) (*fail_cnt)++;
        return NULL;
    }
    if (!jlos_task_manager_add_task(s_mgr, t)) {
        jlos_task_free(s_mgr, t);
        jlos_kfree(t);
        if (fail_cnt) (*fail_cnt)++;
        return NULL;
    }
    return t;
}

static jlos_task_t *spawn_kernel_task(jlos_mmu_t *mmu,
                                      void (*fn)(void),
                                      const char *name,
                                      int *fail_cnt)
{
    jlos_task_t *t = (jlos_task_t *)jlos_kalloc(sizeof(*t));
    if (!t) { if (fail_cnt) (*fail_cnt)++; return NULL; }
    if (jlos_task_init(t, mmu, fn, name) < 0) {
        jlos_kfree(t);
        if (fail_cnt) (*fail_cnt)++;
        return NULL;
    }
    if (!jlos_task_manager_add_task(s_mgr, t)) {
        jlos_task_free(s_mgr, t);
        jlos_kfree(t);
        if (fail_cnt) (*fail_cnt)++;
        return NULL;
    }
    return t;
}

/* ---- 主入口 ---- */
void multitask_test(jlos_mmu_t *mmu, jlos_task_manager_t *task_manager_)
{
    s_mgr = task_manager_;

    printk_info("=== multitask test start ===\n");
    printk_info("MAX_TASKS=%u KSTACK=%uB USTACK=%uKB MLFQ=%u lv\n",
           JLOS_TASK_MAX_NUM, JLOS_TASK_STACK_SIZE,
           JLOS_TASK_USER_STACK_SIZE / 1024, JLOS_TASK_MLFQ_LEVELS);

    v_t1_a = v_t1_b = 0;
    v_t2_child_pid = v_t2_exit_code = v_t2_fork_done = 0;
    v_t2_parent_done = 0;
    for (int i = 0; i < T3_CHILDREN; i++) v_t3_codes[i] = 0;
    for (int i = 0; i < T3_CHILDREN; i++) v_t3_child_pids[i] = 0;
    v_t3_done = v_t3_fork_done = 0;
    v_t4_ring3_exited = 0;

    int spawn_fails = 0;

    printk_info("[test 1] schedule alternation (2 kernel tasks)\n");
    spawn_kernel_task(mmu, test1_entry_a, "t1_a", &spawn_fails);
    spawn_kernel_task(mmu, test1_entry_b, "t1_b", &spawn_fails);
    printk_info("[test 2] fork -> wait -> exit_code (1 child, magic=%u)\n", T2_MAGIC_EXIT);
    spawn_kernel_task(mmu, test2_parent_entry, "t2_parent", &spawn_fails);
    printk_info("[test 3] fork %u children (exit_code = 100..%u)\n",
           T3_CHILDREN, 100u + T3_CHILDREN - 1);
    spawn_kernel_task(mmu, test3_parent_entry, "t3_parent", &spawn_fails);
    printk_info("[test 4] ring3 user task smoke\n");
    spawn_user_task(mmu, test4_ring3_entry, "t4_ring3", &spawn_fails);

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
        int all = (v_t1_a >= 500 && v_t1_b >= 500)
               && (v_t2_parent_done == 1)
               && (v_t3_done == 1)
               && (v_t4_ring3_exited >= 1);
        if (all) break;
        if (now - t0 >= TIMEOUT_TICKS) break;
        for (volatile int k = 0; k < 20000; k++);
    }
    printk_info("budget: elapsed=%u ticks (start=%u now=%u)\n",
           (unsigned)(now - t0), (unsigned)t0, (unsigned)now);

    int fails = 0;
    printk_info("subcase results:\n");

    printk_info("[1] alternation: A=%d B=%d -> ", v_t1_a, v_t1_b);
    if (v_t1_a >= 500 && v_t1_b >= 500)  printk_info("PASS\n");
    else { printk_err("FAIL (both >= 500)\n"); fails++; }

    printk_info("[2] fork-wait: status=%d pid=%u exit=%u -> ",
           v_t2_parent_done, (unsigned)v_t2_child_pid, (unsigned)v_t2_exit_code);
    if (v_t2_parent_done == 1 && v_t2_child_pid > 0
        && v_t2_exit_code == T2_MAGIC_EXIT)  printk_info("PASS\n");
    else { printk_err("FAIL (want status=1, pid>0, exit=%u)\n", T2_MAGIC_EXIT); fails++; }

    printk_info("[3] 10-child pressure: status=%d\n", v_t3_done);
    if (v_t3_done == 1) {
        int bad = 0;
        for (int i = 0; i < T3_CHILDREN; i++) {
            if (v_t3_codes[i] != 100u + (uint32_t)i) {
                printk_err("mismatch i=%u exit=%u expect=%u\n",
                       i, (unsigned)v_t3_codes[i], 100u + i);
                bad++;
            }
        }
        if (!bad)  printk_info("all 10 exit_codes matched, PASS\n");
        else { printk_err("%u mismatch, FAIL\n", bad); fails++; }
    } else { printk_err("FAIL status=%d\n", v_t3_done); fails++; }

    printk_info("[4] ring3 smoke: exited=%d -> ", v_t4_ring3_exited);
    if (v_t4_ring3_exited >= 1)  printk_info("PASS\n");
    else { printk_err("FAIL\n"); fails++; }

    if (!fails) printk_info("multitask: ALL PASSED\n");
    else        printk_err("multitask: FAILS: %d, check above\n", fails);

teardown:
    ;
    int i = 0;
    while ((i = s_mgr->num_tasks - 1) > 0) {
        jlos_task_t *t = s_mgr->tasks[i];
        if (!t || t == s_mgr->idle_task) continue;
        if (!jlos_list_empty(&t->zombie_node)) {
            jlos_list_del_init(&t->zombie_node);
        }
        jlos_task_free(s_mgr, t);
    }
}
