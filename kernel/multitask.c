#include <kernel/multitask.h>
#include <hal/context.h>

extern jlos_task_t *g_current_task_ptr;

void jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void))
{
    self->m_status = JLOS_TASK_RUNNING;
    self->m_saved_esp = 0;
    self->cpustate.m_eax = 0;
    self->cpustate.m_ebx = 0;
    self->cpustate.m_ecx = 0;
    self->cpustate.m_edx = 0;
    self->cpustate.m_esi = 0;
    self->cpustate.m_edi = 0;
    self->cpustate.m_ebp = 0;
    self->cpustate.m_error = 0;
    self->cpustate.m_eip = 0;
    self->cpustate.m_cs = 0;
    self->cpustate.m_eflags = 0x002;
    jlos_arch_task_init_arch(&self->cpustate, mmu, entrypoint, self->stack);
}

void jlos_task_destroy(jlos_task_t* self)
{
}

void jlos_task_manager_init(jlos_task_manager_t* self)
{
    self->m_num_tasks = 0;
    self->m_current_task = -1;
    self->main_thread_saved = false;
}

void jlos_task_manager_destroy(jlos_task_manager_t* self)
{
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
            self->main_thread_state = *cpustate;
            self->main_thread_esp = (uint32_t)cpustate;
            self->main_thread_saved = true;
        }
    } else if (self->m_current_task < (int)self->m_num_tasks) {
        /* 如果当前是任务，保存任务的状态 */
        jlos_task_t *current = self->tasks[self->m_current_task];
        if (current != NULL && current->m_status == JLOS_TASK_RUNNING) {
            current->cpustate = *cpustate;
            current->m_saved_esp = (uint32_t)cpustate;
        }
    }

    /* 寻找下一个 RUNNING 任务，最多扫 2 圈防止所有任务都 TERMINATED 时死循环 */
    int start_idx = self->m_current_task;
    for (int i = 0; i < self->m_num_tasks * 2 + 1; i++) {
        if (++self->m_current_task >= self->m_num_tasks) {
            self->m_current_task %= self->m_num_tasks;
        }
        jlos_task_t *next = self->tasks[self->m_current_task];
        if (next != NULL && next->m_status == JLOS_TASK_RUNNING) {
            g_current_task_ptr = next;
            if (next->m_saved_esp == 0) {
                return &next->cpustate;
            }
            uint32_t *src = (uint32_t*)&next->cpustate;
            uint32_t *dst = (uint32_t*)next->m_saved_esp;
            for (int k = 0; k < 11; k++) dst[k] = src[k];
            return (jlos_cpu_state_t*)next->m_saved_esp;
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
        uint32_t *src = (uint32_t*)&self->main_thread_state;
        uint32_t *dst = (uint32_t*)self->main_thread_esp;
        for (int k = 0; k < 11; k++) dst[k] = src[k];
        return (jlos_cpu_state_t*)self->main_thread_esp;
    }
    return cpustate;
}