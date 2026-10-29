/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/kthread.h>
#include <kernel/memory_manager.h>
#include <kernel/initcall.h>
#include <hal/hal.h>

#define JLOS_KERNEL_LOG_SUBSYS "kthread"
#include <kernel/printk.h>

static jlos_task_t *s_kthread_daemon;
static jlos_list_head_t s_kthread_daemon_queue = JLOS_LIST_HEAD_INIT(s_kthread_daemon_queue);
static jlos_spinlock_t s_kthread_daemon_lock = JLOS_SPINLOCK_INIT;
static jlos_semaphore_t s_kthread_daemon_work;

static void jlos_kthread_entry(void)
{
    jlos_task_t *self = jlos_hal_current_task();
    jlos_kthread_t *k = (jlos_kthread_t *)self->kthread;
    k->ret_val = k->tf(k->data);
    k->exited = true;
    jlos_semaphore_post(&k->exited_sem);
}

static jlos_task_t *jlos_kthread_alloc(thread_fn tf, void *data, const char *name, uint32_t parent_pid)
{
    jlos_task_t *task = (jlos_task_t *)jlos_kalloc(sizeof(jlos_task_t));
    if (!task) {
        return nullptr;
    }
    jlos_kthread_t *k = (jlos_kthread_t *)jlos_kalloc(sizeof(jlos_kthread_t));
    if (!k) {
        jlos_kfree(task);
        return nullptr;
    }
    k->tf = tf;
    k->data = data;
    k->ret_val = 0;
    k->should_stop = false;
    k->exited = false;
    k->stop_in_progress = false;
    jlos_semaphore_init(&k->exited_sem, 0);
    if (jlos_task_init(task, nullptr, jlos_kthread_entry, name)) {
        jlos_kfree(k);
        jlos_task_free(g_task_manager_ptr, task);
        return nullptr;
    }
    task->kthread = k;
    task->parent_pid = parent_pid;
    if (!jlos_task_manager_add_task(g_task_manager_ptr, task)) {
        task->kthread = nullptr;
        jlos_task_free(g_task_manager_ptr, task);
        jlos_kfree(k);
        return nullptr;
    }
    return task;
}

static int32_t jlos_kthread_daemon_main(void *arg)
{
    (void)arg;
    for (;;) {
        jlos_semaphore_wait(&s_kthread_daemon_work);
        for (;;) {
            uint32_t fl = jlos_spin_lock_irqsave(&s_kthread_daemon_lock);
            if (jlos_list_empty(&s_kthread_daemon_queue)) {
                jlos_spin_unlock_irqrestore(&s_kthread_daemon_lock, fl);
                break;
            }
            jlos_list_head_t *first = s_kthread_daemon_queue.next;
            jlos_list_del_init(first);
            jlos_spin_unlock_irqrestore(&s_kthread_daemon_lock, fl);
            jlos_kthread_create_info_t *info = container_of(first, jlos_kthread_create_info_t, node);
            info->task = jlos_kthread_alloc(info->tf, info->data, info->name, s_kthread_daemon->pid);
            jlos_semaphore_post(&info->done);
        }
    }
    return 0;
}

static void jlos_kthread_daemon_entry(void)
{
    jlos_kthread_daemon_main(nullptr);
}

jlos_task_t *jlos_kthread_create(thread_fn tf, void *data, const char *name)
{
    if (!tf || !s_kthread_daemon) {
        return nullptr;
    }
    jlos_kthread_create_info_t info;
    info.tf = tf;
    info.data = data;
    info.task = nullptr;
    jlos_semaphore_init(&info.done, 0);
    jlos_list_init(&info.node);
    if (name) {
        uint32_t i;
        for (i = 0; i < JLOS_TASK_NAME_SIZE - 1 && name[i]; i++) {
            info.name[i] = name[i];
        }
        info.name[i] = '\0';
    } else {
        info.name[0] = '\0';
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_kthread_daemon_lock);
    jlos_list_add(&info.node, &s_kthread_daemon_queue);
    jlos_spin_unlock_irqrestore(&s_kthread_daemon_lock, fl);

    jlos_semaphore_post(&s_kthread_daemon_work);
    jlos_semaphore_wait(&info.done);
    return info.task;
}

jlos_task_t *jlos_kthread_run(thread_fn tf, void *data, const char *name)
{
    jlos_task_t *task = jlos_kthread_create(tf, data, name);
    if (task) {
        jlos_kthread_wakeup(task);
    }
    return task;
}

void jlos_kthread_wakeup(jlos_task_t *task)
{
    if (task) {
        jlos_task_manager_enqueue(g_task_manager_ptr, task);
    }
}

int32_t jlos_kthread_stop(jlos_task_t *task)
{
    if (!task || !task->kthread) {
        return 0;
    }
    jlos_kthread_t *k = (jlos_kthread_t *)task->kthread;
    k->stop_in_progress = true;
    k->should_stop = true;
    jlos_task_wakeup(task);
    jlos_semaphore_wait(&k->exited_sem);
    int32_t ret = k->ret_val;
    k->stop_in_progress = false;
    return ret;
}

bool jlos_kthread_should_stop(void)
{
    jlos_task_t *self = jlos_hal_current_task();
    jlos_kthread_t *k = self ? (jlos_kthread_t *)self->kthread : nullptr;
    return k && k->should_stop;
}

bool jlos_kthread_can_release(jlos_task_t *task)
{
    jlos_kthread_t *k = (jlos_kthread_t *)task->kthread;
    return !k || !k->stop_in_progress;
}

void jlos_kthread_release(jlos_task_t *task)
{
    jlos_kthread_t *k = (jlos_kthread_t *)task->kthread;
    if (k) {
        jlos_kfree(k);
        task->kthread = nullptr;
    }
}

static void jlos_kthread_daemon_init(void)
{
    jlos_semaphore_init(&s_kthread_daemon_work, 0);
    jlos_task_t *task = (jlos_task_t *)jlos_kalloc(sizeof(jlos_task_t));
    if (!task) {
        printk_err("task alloc failed\n");
        return;
    }
    if (jlos_task_init(task, nullptr, jlos_kthread_daemon_entry, "kth_daemon")) {
        jlos_kfree(task);
        printk_err("task init failed\n");
        return;
    }
    task->parent_pid = 0;
    if (!jlos_task_manager_add_task(g_task_manager_ptr, task)) {
        jlos_task_free(g_task_manager_ptr, task);
        printk_err("add task to tmgr failed\n");
        return;
    }
    s_kthread_daemon = task;
    jlos_task_manager_enqueue(g_task_manager_ptr, task);
    printk_info("kth_daemon init success\n");
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_kthread_daemon_init);
