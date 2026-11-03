/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/workqueue.h>
#include <kernel/kthread.h>
#include <kernel/initcall.h>
#include <kernel/memory_manager.h>
#include <hal/hal.h>

#define JLOS_KERNEL_LOG_SUBSYS "wk_que"
#include <kernel/printk.h>

static jlos_workqueue_t *s_system_workqueue;

static int32_t jlos_workqueue_worker(void *arg)
{
    jlos_workqueue_cpu_t *qc = (jlos_workqueue_cpu_t *)arg;
    for (;;) {
        jlos_semaphore_wait(&qc->work_sem);
        for (;;) {
            uint32_t fl = jlos_spin_lock_irqsave(&qc->lock);
            if (jlos_list_empty(&qc->list)) {
                jlos_spin_unlock_irqrestore(&qc->lock, fl);
                break;
            }
            jlos_list_head_t *first = qc->list.next;
            jlos_list_del_init(first);
            jlos_spin_unlock_irqrestore(&qc->lock, fl);
            jlos_work_t *work = container_of(first, jlos_work_t, entry);
            jlos_atomic_set(&work->pending, 0);
            work->func(work);
        }
        if (qc->stop) {
            jlos_semaphore_post(&qc->exited_sem);
            return 0;
        }
    }
}

static void jlos_workqueue_barrier_func(jlos_work_t *work)
{
    jlos_workqueue_barrier_t *barrier = container_of(work, jlos_workqueue_barrier_t, work);
    jlos_semaphore_post(&barrier->done);
}

void jlos_workqueue_init_work(jlos_work_t *work, wk_func wf)
{
    work->func = wf;
    jlos_list_init(&work->entry);
    jlos_atomic_set(&work->pending, 0);
}

bool jlos_queue_work_on(uint32_t cpu, jlos_workqueue_t *wq, jlos_work_t *work)
{
    if (!wq || !work || !work->func || cpu >= jlos_hal_num_cpus()) {
        return false;
    }
    if (jlos_atomic_xchg(&work->pending, 1)) {
        return false;
    }
    jlos_workqueue_cpu_t *qc = &wq->per_cpu[cpu];
    uint32_t fl = jlos_spin_lock_irqsave(&qc->lock);
    jlos_list_add_tail(&work->entry, &qc->list);
    jlos_spin_unlock_irqrestore(&qc->lock, fl);
    jlos_semaphore_post(&qc->work_sem);
    return true;
}

void jlos_flush_workqueue(jlos_workqueue_t *wq)
{
    if (!wq) {
        return;
    }
    uint32_t num_cpus = jlos_hal_num_cpus();
    for (uint32_t i = 0; i < num_cpus; i++) {
        jlos_workqueue_barrier_t barrier;
        jlos_workqueue_init_work(&barrier.work, jlos_workqueue_barrier_func);
        jlos_semaphore_init(&barrier.done, 0);
        if (!jlos_queue_work_on(i, wq, &barrier.work)) {
            continue;
        }
        jlos_semaphore_wait(&barrier.done);
    }
}

jlos_workqueue_t *jlos_workqueue_create(const char *name)
{
    jlos_workqueue_t *wq = (jlos_workqueue_t *)jlos_kalloc(sizeof(jlos_workqueue_t));
    if (!wq) {
        printk_err("wq alloc failed\n");
        return nullptr;
    }
    for (uint32_t i = 0; i < JLOS_MAX_CPUS; i++) {
        jlos_list_init(&wq->per_cpu[i].list);
        jlos_spinlock_init(&wq->per_cpu[i].lock);
        jlos_semaphore_init(&wq->per_cpu[i].work_sem, 0);
        jlos_semaphore_init(&wq->per_cpu[i].exited_sem, 0);
        wq->per_cpu[i].stop = false;
        wq->per_cpu[i].worker = nullptr;
    }
    uint32_t num_cpus = jlos_hal_num_cpus();
    for (uint32_t i = 0; i < num_cpus; i++) {
        wq->per_cpu[i].worker = jlos_kthread_create(jlos_workqueue_worker, &wq->per_cpu[i], name);
        if (!wq->per_cpu[i].worker) {
            printk_err("wq worker create failed cpu = %u\n", i);
            for (uint32_t j = 0; j < i; j++) {
                wq->per_cpu[j].stop = true;
                jlos_semaphore_post(&wq->per_cpu[j].work_sem);
            }
            for (uint32_t j = 0; j < i; j++) {
                jlos_semaphore_wait(&wq->per_cpu[j].exited_sem);
            }
            jlos_kfree(wq);
            return nullptr;
        }
        jlos_kthread_wakeup(wq->per_cpu[i].worker);
    }
    printk_info("wq create success\n");
    return wq;
}

void jlos_workqueue_destroy(jlos_workqueue_t *wq)
{
    if (!wq) {
        return;
    }
    jlos_flush_workqueue(wq);
    uint32_t num_cpus = jlos_hal_num_cpus();
    for (uint32_t i = 0; i < num_cpus; i++) {
        wq->per_cpu[i].stop = true;
        jlos_semaphore_post(&wq->per_cpu[i].work_sem);
    }
    for (uint32_t i = 0; i < num_cpus; i++) {
        jlos_semaphore_wait(&wq->per_cpu[i].exited_sem);
    }
    jlos_kfree(wq);
    printk_info("wq destroy done\n");
}

jlos_workqueue_t *jlos_system_workqueue(void)
{
    return s_system_workqueue;
}

static void jlos_workqueue_init_system(void)
{
    s_system_workqueue = jlos_workqueue_create("system");
    if (!s_system_workqueue) {
        printk_err("sys_wq init failed\n");
        return;
    }
    printk_info("sys_wq init success\n");
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, jlos_workqueue_init_system);
