/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_KERNEL_KTHREAD_H
#define _JLOS_KERNEL_KTHREAD_H

#include <common/types.h>
#include <kernel/multitask.h>
#include <kernel/sync.h>

typedef int (*thread_fn)(void *);

typedef struct {
    thread_fn           tf;
    void                *data;
    int32_t             ret_val;
    volatile bool       should_stop;
    volatile bool       exited;
    volatile bool       stop_in_progress;
    jlos_semaphore_t    exited_sem;
} jlos_kthread_t;

typedef struct {
    thread_fn           tf;
    void                *data;
    char                name[JLOS_TASK_NAME_SIZE];
    jlos_task_t         *task;
    jlos_semaphore_t    done;
    jlos_list_head_t    node;
} jlos_kthread_create_info_t;

jlos_task_t *jlos_kthread_create(thread_fn tf, void *data, const char *name);
jlos_task_t *jlos_kthread_run(thread_fn tf, void *data, const char *name);
void jlos_kthread_wakeup(jlos_task_t *task);
int32_t jlos_kthread_stop(jlos_task_t *task);
bool jlos_kthread_should_stop(void);

bool jlos_kthread_can_release(jlos_task_t *task);
void jlos_kthread_release(jlos_task_t *task);

#endif
