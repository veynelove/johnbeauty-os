#include <kernel/sync.h>

extern jlos_task_t *g_current_task_ptr;

static void wait_enqueue(jlos_task_t **head, jlos_task_t **tail, jlos_task_t *task)
{
    task->next_wait = NULL;
    if (*tail) {
        (*tail)->next_wait = task;
    } else {
        *head = task;
    }
    *tail = task;
}

static jlos_task_t *wait_dequeue(jlos_task_t **head, jlos_task_t **tail)
{
    jlos_task_t *task = *head;
    if (task) {
        *head = task->next_wait;
        if (!*head) {
            *tail = NULL;
        }
        task->next_wait = NULL;
    }
    return task;
}

void jlos_semaphore_init(jlos_semaphore_t *sem, int32_t init_count)
{
    sem->resource_count = init_count;
    sem->wait_head = NULL;
    sem->wait_tail = NULL;
    jlos_spinlock_init(&sem->lock);
}

void jlos_semaphore_wait(jlos_semaphore_t *sem)
{
    if (!g_current_task_ptr) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&sem->lock);
    sem->resource_count--;
    if (sem->resource_count >= 0) {
        jlos_spin_unlock_irqrestore(&sem->lock, flags);
        return;
    }
    wait_enqueue(&sem->wait_head, &sem->wait_tail, g_current_task_ptr);

    JLOS_TASK_SET_BLOCKED(g_current_task_ptr);
    jlos_spin_unlock_irqrestore(&sem->lock, flags);
}

void jlos_semaphore_post(jlos_semaphore_t *sem)
{
    uint32_t flags = jlos_spin_lock_irqsave(&sem->lock);
    sem->resource_count++;
    if (sem->resource_count > 0) {
        jlos_spin_unlock_irqrestore(&sem->lock, flags);
        return;
    }
    jlos_task_t *wait = wait_dequeue(&sem->wait_head, &sem->wait_tail);
    if (wait && wait->status == JLOS_TASK_BLOCKED) {
        JLOS_TASK_SET_READY(wait);
    }
    jlos_spin_unlock_irqrestore(&sem->lock, flags);
}

void jlos_mutex_init(jlos_mutex_t *mutex)
{
    mutex->locked = 0;
    mutex->owner = NULL;
    mutex->recursion = 0;
    mutex->wait_head = NULL;
    mutex->wait_tail = NULL;
    jlos_spinlock_init(&mutex->lock);
}

void jlos_mutex_lock(jlos_mutex_t *mutex)
{
    if (!g_current_task_ptr) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&mutex->lock);
    if (mutex->owner == g_current_task_ptr) {
        mutex->recursion++;
        jlos_spin_unlock_irqrestore(&mutex->lock, flags);
        return;
    }
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = g_current_task_ptr;
        mutex->recursion = 1;
        jlos_spin_unlock_irqrestore(&mutex->lock, flags);
        return;
    }
    wait_enqueue(&mutex->wait_head, &mutex->wait_tail, g_current_task_ptr);
    
    JLOS_TASK_SET_BLOCKED(g_current_task_ptr);
    jlos_spin_unlock_irqrestore(&mutex->lock, flags);
}

void jlos_mutex_unlock(jlos_mutex_t *mutex)
{
    uint32_t flags = jlos_spin_lock_irqsave(&mutex->lock);
    if (!g_current_task_ptr || mutex->owner != g_current_task_ptr) {
        jlos_spin_unlock_irqrestore(&mutex->lock, flags);
        return;
    }
    mutex->recursion--;
    if (mutex->recursion > 0) {
        jlos_spin_unlock_irqrestore(&mutex->lock, flags);
        return;
    }
    mutex->owner = NULL;
    mutex->locked = 0;
    jlos_task_t *wait = wait_dequeue(&mutex->wait_head, &mutex->wait_tail);
    if (wait && wait->status == JLOS_TASK_BLOCKED) {
        mutex->locked = 1;
        mutex->owner = wait;
        mutex->recursion = 1;
        JLOS_TASK_SET_READY(wait);
    }
    jlos_spin_unlock_irqrestore(&mutex->lock, flags);
}

void jlos_cond_init(jlos_cond_t *cond)
{
    cond->wait_head = NULL;
    cond->wait_tail = NULL;
    jlos_spinlock_init(&cond->lock);
}

void jlos_cond_wait(jlos_cond_t *cond, jlos_mutex_t *mutex)
{
    if (!g_current_task_ptr) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&cond->lock);
    wait_enqueue(&cond->wait_head, &cond->wait_tail, g_current_task_ptr);

    JLOS_TASK_SET_BLOCKED(g_current_task_ptr);
    jlos_spin_unlock_irqrestore(&cond->lock, flags);
    jlos_mutex_unlock(mutex);
}

void jlos_cond_signal(jlos_cond_t *cond)
{
    uint32_t flags = jlos_spin_lock_irqsave(&cond->lock);
    jlos_task_t *wait = wait_dequeue(&cond->wait_head, &cond->wait_tail);
    if (wait && wait->status == JLOS_TASK_BLOCKED) {
        JLOS_TASK_SET_READY(wait);
    }
    jlos_spin_unlock_irqrestore(&cond->lock, flags);
}

void jlos_cond_broadcast(jlos_cond_t *cond)
{
    uint32_t flags = jlos_spin_lock_irqsave(&cond->lock);
    jlos_task_t *wait = NULL;
    while ((wait = wait_dequeue(&cond->wait_head, &cond->wait_tail))) {
        if (wait->status == JLOS_TASK_BLOCKED) {
            JLOS_TASK_SET_READY(wait);
        }
    }
    jlos_spin_unlock_irqrestore(&cond->lock, flags);
}
