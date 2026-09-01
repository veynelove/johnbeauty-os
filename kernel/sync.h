#ifndef JLOS_KERNEL_SYNC_H
#define JLOS_KERNEL_SYNC_H

#include <common/types.h>
#include <kernel/multitask.h>
#include <hal/spinlock.h>

typedef struct {
    volatile int32_t    resource_count;
    jlos_task_t         *wait_head;
    jlos_task_t         *wait_tail;
    jlos_spinlock_t     lock;
} jlos_semaphore_t;

typedef struct {
    volatile uint32_t   locked;
    jlos_task_t         *owner;
    uint32_t            recursion;
    jlos_task_t         *wait_head;
    jlos_task_t         *wait_tail;
    jlos_spinlock_t     lock;
} jlos_mutex_t;

typedef struct {
    jlos_task_t     *wait_head;
    jlos_task_t     *wait_tail;
    jlos_spinlock_t lock;
} jlos_cond_t;

void jlos_semaphore_init(jlos_semaphore_t *sem, int32_t init_count);
void jlos_semaphore_wait(jlos_semaphore_t *sem);
void jlos_semaphore_post(jlos_semaphore_t *sem);

void jlos_mutex_init(jlos_mutex_t *mutex);
void jlos_mutex_lock(jlos_mutex_t *mutex);
void jlos_mutex_unlock(jlos_mutex_t *mutex);

void jlos_cond_init(jlos_cond_t *cond);
void jlos_cond_wait(jlos_cond_t *cond, jlos_mutex_t *mutex);
void jlos_cond_signal(jlos_cond_t *cond);
void jlos_cond_broadcast(jlos_cond_t *cond);
#endif
