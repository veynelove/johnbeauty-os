#include <kernel/multitask.h>

void jlos_task_init(jlos_task_t* self, jlos_gdt_t *gdt, void (*entrypoint)(void))
{
    self->cpustate = (jlos_cpu_state_t *)(self->stack + 4096 - sizeof(jlos_cpu_state_t));
    self->cpustate->m_eax = 0;
    self->cpustate->m_ebx = 0;
    self->cpustate->m_ecx = 0;
    self->cpustate->m_edx = 0;

    self->cpustate->m_esi = 0;
    self->cpustate->m_edi = 0;
    self->cpustate->m_ebp = 0;

    self->cpustate->m_eip = (uint32_t)entrypoint;
    self->cpustate->m_cs = jlos_gdt_code_segment_selector(gdt);
    self->cpustate->m_eflags = 0x202;
}

void jlos_task_destroy(jlos_task_t* self)
{
}

void jlos_task_manager_init(jlos_task_manager_t* self)
{
    self->m_num_tasks = 0;
    self->m_current_task = -1;
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
    if (self->m_num_tasks <= 0) {
        return cpustate;
    }
    if (self->m_current_task >= 0) {
        self->tasks[self->m_current_task]->cpustate = cpustate;
    }
    if (++self->m_current_task >= self->m_num_tasks) {
        self->m_current_task %= self->m_num_tasks;
    }
    return self->tasks[self->m_current_task]->cpustate;
}