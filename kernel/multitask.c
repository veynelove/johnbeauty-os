#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <hal/context.h>
#include <hal/timer.h>
#include <kernel/printk.h>

extern jlos_task_t *g_current_task_ptr;

jlos_task_manager_t *g_task_manager_ptr = NULL;

static uint32_t s_next_pid = 1;

void jlos_task_init_1(jlos_task_t *self, const char *name)
{
    if (name) {
        int i;
        for (i = 0; i < JLOS_TASK_NAME_SIZE && name[i]; i++) {
            self->m_name[i] = name[i];
        }
        self->m_name[i] = '\0';
    } else {
        self->m_name[0] = '\0';
    }
    self->m_status = JLOS_TASK_READY;
    self->m_pid = s_next_pid++;
    self->m_parent_pid = 0;
    self->m_exit_code = 0;
    self->m_mm = NULL;
    self->m_sleeping = false;
    self->m_wake_tick = jlos_hal_timer_get_ticks();
    self->m_yield = false;
    self->m_errno = 0;
    self->m_waiting_pid = self->m_pid;
    self->m_last_ready_tick = jlos_hal_timer_get_ticks();
    self->m_next_waiter = NULL;
}

void jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name)
{
    jlos_task_init_1(self, name);
    self->m_stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->m_stack_size = JLOS_TASK_STACK_SIZE;
    self->m_is_user_process = false;
    self->m_priority = 0;
    self->m_default_slice = (2 << self->m_priority);
    self->m_remain_slice = self->m_default_slice;
    self->m_user_stack = NULL;
    self->m_user_stack_size = 0;
    if (self->m_stack) {
        jlos_memset(self->m_stack, 0, JLOS_TASK_STACK_SIZE);
    }

    jlos_cpu_state_init(&self->cpustate);
    jlos_arch_task_init_arch(&self->cpustate, mmu, entrypoint, self->m_stack, self->m_stack_size);
}

void jlos_task_init_user(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name)
{
    jlos_task_init_1(self, name);
    self->m_stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->m_stack_size = JLOS_TASK_STACK_SIZE;
    self->m_user_stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->m_user_stack_size = JLOS_TASK_STACK_SIZE;
    self->m_is_user_process = true;
    self->m_priority = 0;
    self->m_default_slice = (2 << self->m_priority);
    self->m_remain_slice = self->m_default_slice;
    if (self->m_stack) {
        jlos_memset(self->m_stack, 0, JLOS_TASK_STACK_SIZE);
    }
    if (self->m_user_stack) {
        jlos_memset(self->m_user_stack, 0, JLOS_TASK_STACK_SIZE);
        jlos_paging_change_flags_range(jlos_active_paging_context,
            (uint32_t)self->m_user_stack,
            (uint32_t)(self->m_user_stack + JLOS_TASK_STACK_SIZE),
            JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE | JLOS_PTE_USER);
    }
    uint32_t entry_start = (uint32_t)entrypoint & ~0xFFF;
    uint32_t entry_end = entry_start + 4096;
    jlos_paging_change_flags_range(jlos_active_paging_context,
        entry_start, entry_end,
        JLOS_PTE_PRESENT | JLOS_PTE_USER);

    jlos_cpu_state_init(&self->cpustate);

    uint32_t user_stack_top = self->m_user_stack ? (uint32_t)(self->m_user_stack + JLOS_TASK_STACK_SIZE) : 0;
    uint16_t user_ss = 0x2B;
    jlos_arch_task_init_arch_user(&self->cpustate, mmu, entrypoint,
        self->m_stack, self->m_stack_size, user_stack_top, user_ss);
}

void jlos_task_destroy(jlos_task_t* self)
{
    if (self->m_stack) {
        jlos_free(self->m_stack);
        self->m_stack = NULL;
    }
    if (self->m_user_stack) {
        jlos_free(self->m_user_stack);
        self->m_user_stack = NULL;
    }
}

void jlos_task_manager_init(jlos_task_manager_t* self)
{
    self->m_num_tasks = 0;
    self->m_current_task = -1;
    self->main_thread_saved = false;
    g_task_manager_ptr = self;
}

void jlos_task_manager_destroy(jlos_task_manager_t* self)
{
    self->m_num_tasks = 0;
    self->m_current_task = -1;
    self->main_thread_saved =false;
    g_task_manager_ptr = NULL;
}

bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task)
{
    if (self->m_num_tasks >= 256) {
        return false;
    }
    self->tasks[self->m_num_tasks++] = task;
    return true;
}

jlos_cpu_state_t *jlos_task_manager_schedule(jlos_task_manager_t* self, jlos_cpu_state_t *cpustate)
{
    if (self == NULL || cpustate == NULL) return cpustate;
    if (self->m_num_tasks <= 0) {
        g_current_task_ptr = NULL;
        return cpustate;
    }

    /* 如果当前是主线程（m_current_task == -1），保存主线程的状态 */
    if (self->m_current_task == -1) {
        if (!self->main_thread_saved) {
            jlos_memcpy(&self->main_thread_state, cpustate, sizeof(jlos_cpu_state_t));
            if (!jlos_cpu_state_is_user_mode(cpustate)) {
                jlos_cpu_state_record_user_stack(&self->main_thread_state, cpustate);
            }
            self->main_thread_saved = true;
        }
    } else if (self->m_current_task < (int)self->m_num_tasks) {
        jlos_task_t *current = self->tasks[self->m_current_task];
        if (current && current->m_status == JLOS_TASK_RUNNING) {
            if (current->m_yield || current->m_sleeping) {
                current->m_status = JLOS_TASK_READY;
                current->m_last_ready_tick = jlos_hal_timer_get_ticks();
            } else
            if (current->m_remain_slice == 0) {
                if (current->m_priority < JLOS_TASK_MLFQ_LEVELS - 1) {
                    current->m_priority++;
                    current->m_default_slice = 2 << current->m_priority;
                }
                current->m_status = JLOS_TASK_READY;
                current->m_last_ready_tick = jlos_hal_timer_get_ticks();
            }
            jlos_memcpy(&current->cpustate, cpustate, sizeof(jlos_cpu_state_t));
            if (!jlos_cpu_state_is_user_mode(cpustate)) {
                jlos_cpu_state_record_user_stack(&current->cpustate, cpustate);
            }
        }
    }
    uint32_t now_tick = jlos_hal_timer_get_ticks();
    for (int i = 0; i < self->m_num_tasks; i++) {
        jlos_task_t *t = self->tasks[i];
        if (!t) {
            continue;
        }
        if (t->m_sleeping && t->m_wake_tick <= now_tick) {
            t->m_sleeping = false;
            t->m_wake_tick = now_tick;
        }
        if (t->m_status == JLOS_TASK_WAITING && t->m_waiting_pid != t->m_pid) {
            for (int j = 0; j < self->m_num_tasks; j++) {
                jlos_task_t *child = self->tasks[j];
                if (child && child->m_pid == t->m_waiting_pid
                && child->m_status == JLOS_TASK_TERMINATED) {
                    t->m_status = JLOS_TASK_READY;
                    t->m_waiting_pid = t->m_pid;
                    t->m_last_ready_tick = now_tick;
                    break;
                }
            }
        }
        if (t->m_status == JLOS_TASK_READY && (now_tick - t->m_last_ready_tick) > JLOS_TASK_MLFQ_AGING_TICKS
        && t->m_priority > 0) {
            t->m_priority--;
            t->m_default_slice = (2 << t->m_priority);
            t->m_last_ready_tick = now_tick;
        }
    }

    for (int le = 0; le < JLOS_TASK_MLFQ_LEVELS; le++) {
        for (int i = 0; i < self->m_num_tasks; i++) {
            int idx = (self->m_current_task + 1 + i) % self->m_num_tasks;
            jlos_task_t *next = self->tasks[idx];
            if (next && next->m_status == JLOS_TASK_READY && next->m_priority == le
            && !next->m_sleeping) {
                self->m_current_task = idx;
                if (next->m_mm) {
                    jlos_paging_switch(next->m_mm);
                }
                next->m_status = JLOS_TASK_RUNNING;
                next->m_remain_slice = next->m_default_slice;
                g_current_task_ptr = next;
                jlos_arch_tss_set_ctx((uint32_t)(next->m_stack + next->m_stack_size));
                return &next->cpustate;
            }
        }
    }

    /* 没有Ready任务，恢复主线程的状态 */
    g_current_task_ptr = NULL;
    self->m_current_task = -1;
    if (self->main_thread_saved) {
        self->main_thread_saved = false;
        return &self->main_thread_state;
    }
    return cpustate;
}

jlos_task_t *jlos_task_manager_curr_task_on_tick(jlos_task_manager_t *self)
{
    if (!self) {
        return NULL;
    }
    if (self->m_current_task < 0 || self->m_current_task >= self->m_num_tasks) {
        return NULL;
    }
    jlos_task_t *curr = self->tasks[self->m_current_task];
    if (curr && curr->m_status == JLOS_TASK_RUNNING) {
        if (curr->m_remain_slice > 0) {
            curr->m_remain_slice--;
        }
    }
    return curr;
}

jlos_task_t *jlos_process_fork(jlos_task_manager_t *self, jlos_task_t *parent)
{
    if (self->m_num_tasks >= 256) {
        return NULL;
    }
    jlos_task_t *child = (jlos_task_t *)jlos_malloc(sizeof(jlos_task_t));
    if (!child) {
        return NULL;
    }
    *child = *parent;
    child->m_stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    child->m_stack_size = JLOS_TASK_STACK_SIZE;
    if (child->m_stack && parent->m_stack) {
        jlos_memcpy(child->m_stack, parent->m_stack, JLOS_TASK_STACK_SIZE);
    }
    child->m_pid = s_next_pid++;
    child->m_parent_pid = parent->m_pid;
    child->m_status = JLOS_TASK_READY;
    child->m_waiting_pid = child->m_pid;
    child->m_sleeping = false;
    child->m_wake_tick = jlos_hal_timer_get_ticks();
    child->m_remain_slice = parent->m_default_slice;
    child->m_last_ready_tick = jlos_hal_timer_get_ticks();
    child->m_next_waiter = NULL;
    child->m_exit_code = 0;
    if (parent->m_mm) {
        child->m_mm = (jlos_paging_context_t *)jlos_malloc(sizeof(jlos_paging_context_t));
        jlos_paging_context_init(child->m_mm);
    } else {
        child->m_mm = NULL;
    }
    self->tasks[self->m_num_tasks++] = child;
    return child;
}

int jlos_process_exec(jlos_task_manager_t *self, jlos_task_t *task, void (*entrypoint)(void))
{
    if (task->m_stack) {
        jlos_free(task->m_stack);
        task->m_stack = NULL;
    }
    if (task->m_user_stack) {
        jlos_free(task->m_user_stack);
        task->m_user_stack = NULL;
    }
    jlos_mmu_t *mmu = jlos_mmu_get_kernel();
    jlos_task_init_user(task, mmu, entrypoint, task->m_name);
    task->m_pid = s_next_pid++;
    task->m_parent_pid = 0;
    task->m_exit_code = 0;
    return 0;
}

void jlos_process_exit(jlos_task_manager_t *self, jlos_task_t *task, uint32_t exit_code)
{
    task->m_status = JLOS_TASK_TERMINATED;
    task->m_exit_code = exit_code;
}
