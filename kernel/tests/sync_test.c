#include <kernel/test.h>
#include <kernel/sync.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/timek.h>
#include <hal/mmu.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_sync"
#include <kernel/printk.h>

static jlos_task_manager_t *s_mgr;

static jlos_task_t *sync_spawn(void (*fn)(void), const char *name)
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

static void sync_reap_zombies(void)
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

static void sync_task_exit(void)
{
    jlos_task_t *me = jlos_task_manager_curr_task_on_tick(s_mgr);
    if (me) {
        jlos_process_exit(me, 0);
    }
}

JLOS_TEST(sync, semaphore_count_down_up)
{
    jlos_semaphore_t sem;
    jlos_semaphore_init(&sem, 2);
    jlos_semaphore_wait(&sem);
    jlos_semaphore_wait(&sem);
    jlos_semaphore_post(&sem);
    jlos_semaphore_post(&sem);
    JLOS_TEST_EQ(sem.resource_count, 2);
}

JLOS_TEST(sync, mutex_lock_unlock)
{
    jlos_mutex_t mutex;
    jlos_mutex_init(&mutex);
    jlos_mutex_lock(&mutex);
    JLOS_TEST_EQ(mutex.locked, 1);
    jlos_mutex_unlock(&mutex);
    JLOS_TEST_EQ(mutex.locked, 0);
}

JLOS_TEST(sync, mutex_recursive_reentry)
{
    jlos_mutex_t mutex;
    jlos_mutex_init(&mutex);
    jlos_mutex_lock(&mutex);
    jlos_mutex_lock(&mutex);
    JLOS_TEST_EQ(mutex.recursion, 2);
    jlos_mutex_unlock(&mutex);
    JLOS_TEST_EQ(mutex.recursion, 1);
    jlos_mutex_unlock(&mutex);
    JLOS_TEST_EQ(mutex.locked, 0);
}

static volatile int g_sem_wait_done = 0;
static jlos_semaphore_t g_sem_block;

static void sem_wait_helper(void)
{
    jlos_semaphore_wait(&g_sem_block);
    g_sem_wait_done = 1;
    sync_task_exit();
}

JLOS_TEST(sync, semaphore_block_and_wake)
{
    s_mgr = g_task_manager_ptr;
    jlos_semaphore_init(&g_sem_block, 0);
    g_sem_wait_done = 0;
    JLOS_ASSERT_NOT_NULL(sync_spawn(sem_wait_helper, "sync_sem_w"));

    uint32_t t0 = jlos_timek_get_ticks();
    while (jlos_timek_get_ticks() - t0 < 100) {
        for (volatile int k = 0; k < 1000; k++);
    }
    jlos_semaphore_post(&g_sem_block);

    t0 = jlos_timek_get_ticks();
    while (!g_sem_wait_done && jlos_timek_get_ticks() - t0 < 500) {
        for (volatile int k = 0; k < 1000; k++);
    }
    JLOS_TEST_TRUE(g_sem_wait_done);
    sync_reap_zombies();
}

static volatile int g_cond_wait_done = 0;
static jlos_cond_t g_cond;
static jlos_mutex_t g_cond_mutex;

static void cond_wait_helper(void)
{
    jlos_mutex_lock(&g_cond_mutex);
    jlos_cond_wait(&g_cond, &g_cond_mutex);
    g_cond_wait_done = 1;
    jlos_mutex_unlock(&g_cond_mutex);
    sync_task_exit();
}

JLOS_TEST(sync, cond_wait_signal)
{
    s_mgr = g_task_manager_ptr;
    jlos_cond_init(&g_cond);
    jlos_mutex_init(&g_cond_mutex);
    g_cond_wait_done = 0;
    JLOS_ASSERT_NOT_NULL(sync_spawn(cond_wait_helper, "sync_cond_w"));

    uint32_t t0 = jlos_timek_get_ticks();
    while (jlos_timek_get_ticks() - t0 < 100) {
        for (volatile int k = 0; k < 1000; k++);
    }
    jlos_cond_signal(&g_cond);

    t0 = jlos_timek_get_ticks();
    while (!g_cond_wait_done && jlos_timek_get_ticks() - t0 < 500) {
        for (volatile int k = 0; k < 1000; k++);
    }
    JLOS_TEST_TRUE(g_cond_wait_done);
    sync_reap_zombies();
}

static volatile int g_cond_bcast_count = 0;
static jlos_cond_t g_cond_bcast;
static jlos_mutex_t g_cond_bcast_mutex;

static void cond_bcast_helper(void)
{
    jlos_mutex_lock(&g_cond_bcast_mutex);
    jlos_cond_wait(&g_cond_bcast, &g_cond_bcast_mutex);
    g_cond_bcast_count++;
    jlos_mutex_unlock(&g_cond_bcast_mutex);
    sync_task_exit();
}

JLOS_TEST(sync, cond_wait_broadcast)
{
    s_mgr = g_task_manager_ptr;
    jlos_cond_init(&g_cond_bcast);
    jlos_mutex_init(&g_cond_bcast_mutex);
    g_cond_bcast_count = 0;
    for (int i = 0; i < 3; i++) {
        JLOS_ASSERT_NOT_NULL(sync_spawn(cond_bcast_helper, "sync_cond_b"));
    }

    uint32_t t0 = jlos_timek_get_ticks();
    while (jlos_timek_get_ticks() - t0 < 200) {
        for (volatile int k = 0; k < 1000; k++);
    }
    jlos_cond_broadcast(&g_cond_bcast);

    t0 = jlos_timek_get_ticks();
    while (g_cond_bcast_count < 3 && jlos_timek_get_ticks() - t0 < 1000) {
        for (volatile int k = 0; k < 1000; k++);
    }
    JLOS_TEST_EQ(g_cond_bcast_count, 3);
    sync_reap_zombies();
}