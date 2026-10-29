/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_KERNEL_WROK_QUEUE_H
#define _JLOS_KERNEL_WROK_QUEUE_H

#include <common/types.h>
#include <kernel/multitask.h>
#include <kernel/sync.h>
#include <dsa/list.h>
#include <hal/spinlock.h>
#include <hal/smp.h>
#include <hal/atomic.h>

typedef struct jlos_work jlos_work_t;
typedef void (*wk_func)(jlos_work_t *work);

typedef struct jlos_work {
    wk_func             func;
    jlos_list_head_t    entry;
    jlos_atomic_t       pending;
} jlos_work_t;

typedef struct {
    jlos_list_head_t    list;
    jlos_spinlock_t     lock;
    jlos_task_t         *worker;
    jlos_semaphore_t    work_sem;
    jlos_semaphore_t    exited_sem;
    bool                stop;
} jlos_workqueue_cpu_t;

typedef struct {
    jlos_workqueue_cpu_t per_cpu[JLOS_MAX_CPUS];
} jlos_workqueue_t;

typedef struct {
    jlos_work_t         work;
    jlos_semaphore_t    done;
} jlos_workqueue_barrier_t;

jlos_workqueue_t *jlos_workqueue_create(const char *name);
void jlos_workqueue_destroy(jlos_workqueue_t *wq);
void jlos_workqueue_init_work(jlos_work_t *work, wk_func wf);
bool jlos_queue_work_on(uint32_t cpu, jlos_workqueue_t *wq, jlos_work_t *work);
void jlos_flush_workqueue(jlos_workqueue_t *wq);
jlos_workqueue_t *jlos_system_workqueue(void);

#endif
