#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <hal/context.h>
#include <hal/timer.h>
#include <kernel/printk.h>

extern jlos_task_t *g_current_task_ptr;

jlos_task_manager_t *g_task_manager_ptr = NULL;

static uint32_t s_next_pid = 1;

void jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name)
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
    self->m_status = JLOS_TASK_RUNNING;
    self->m_stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->m_stack_size = JLOS_TASK_STACK_SIZE;
    self->m_pid = s_next_pid++;
    self->m_parent_pid = 0;
    self->m_exit_code = 0;
    self->m_is_user_process = false;
    self->m_mm = NULL;
    self->m_wake_tick = jlos_hal_timer_get_ticks();
    self->m_sleeping = false;
    self->m_yield = false;
    self->m_errno = 0;
    self->m_waiting_pid = self->m_pid;
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
    if (name) {
        int i;
        for (i = 0; i < JLOS_TASK_NAME_SIZE && name[i]; i++) {
            self->m_name[i] = name[i];
        }
        self->m_name[i] = '\0';
    } else {
        self->m_name[0] = '\0';
    }
    self->m_status = JLOS_TASK_RUNNING;
    self->m_stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->m_stack_size = JLOS_TASK_STACK_SIZE;
    self->m_user_stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->m_user_stack_size = JLOS_TASK_STACK_SIZE;
    self->m_pid = s_next_pid++;
    self->m_parent_pid = 0;
    self->m_exit_code = 0;
    self->m_is_user_process = true;
    self->m_mm = NULL;
    self->m_wake_tick = jlos_hal_timer_get_ticks();
    self->m_sleeping = false;
    self->m_yield = false;
    self->m_errno = 0;
    self->m_waiting_pid = self->m_pid;
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
    if (self->m_num_tasks <= 0 || self->m_current_task >= (int)self->m_num_tasks + 100) {
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
        if (current != NULL && current->m_status == JLOS_TASK_RUNNING) {
            jlos_memcpy(&current->cpustate, cpustate, sizeof(jlos_cpu_state_t));
            if (!jlos_cpu_state_is_user_mode(cpustate)) {
                jlos_cpu_state_record_user_stack(&current->cpustate, cpustate);
            }
        }
    }

    /* 寻找下一个 RUNNING 任务，最多扫 2 圈防止所有任务都 TERMINATED 时死循环 */
    int start_idx = self->m_current_task;
    for (int i = 0; i < self->m_num_tasks * 2 + 1; i++) {
        if (++self->m_current_task >= self->m_num_tasks) {
            self->m_current_task %= self->m_num_tasks;
        }
        jlos_task_t *next = self->tasks[self->m_current_task];
        if (next) {
            if (next->m_sleeping && next->m_wake_tick > jlos_hal_timer_get_ticks()) {
            continue;
            }
            if (next->m_sleeping && next->m_wake_tick <= jlos_hal_timer_get_ticks()) {
                next->m_sleeping = false;
                next->m_wake_tick = jlos_hal_timer_get_ticks();
            }

            if (next->m_status == JLOS_TASK_WAITING && next->m_waiting_pid != next->m_pid) {
                for (int j = 0; j < self->m_num_tasks; j++) {
                    jlos_task_t *child = self->tasks[j];
                    if (child && child->m_pid == next->m_waiting_pid && child->m_status == JLOS_TASK_TERMINATED) {
                        next->m_status = JLOS_TASK_RUNNING;
                        next->m_waiting_pid = next->m_pid;
                        break;
                    }
                }
            }

            if (next->m_status == JLOS_TASK_RUNNING) {
                if (next->m_mm) {
                    jlos_paging_switch(next->m_mm);
                }
                g_current_task_ptr = next;
                uint32_t kstack_top = (uint32_t)(next->m_stack + next->m_stack_size);
                jlos_arch_tss_set_esp0(kstack_top);

                return &next->cpustate;
            }
        }
        
        if (i > 0 && self->m_current_task == start_idx) {
            break;
        }
    }

    /* 所有任务已终止，恢复主线程的状态 */
    g_current_task_ptr = NULL;
    self->m_current_task = -1;
    if (self->main_thread_saved) {
        self->main_thread_saved = false;
        return &self->main_thread_state;
    }
    return cpustate;
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
    child->m_status = JLOS_TASK_RUNNING;
    child->m_waiting_pid = child->m_pid;
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
