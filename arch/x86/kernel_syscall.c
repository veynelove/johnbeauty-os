#include <hal/kernel_syscall.h>
#include <arch/x86/cpu_state.h>
#include <kernel/multitask.h>

extern jlos_task_t *g_current_task_ptr;
extern jlos_task_manager_t *g_task_manager_ptr;

uint32_t jlos_syscall_handler_handle_interrupt(jlos_syscall_handler_t* self, uint32_t ctx)
{
    jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)ctx;
    uint32_t syscall_num = cpu->eax;
    int32_t result = jlos_syscall_do_dispatch(self, syscall_num, cpu->ebx, cpu->ecx, cpu->edx);
    
    cpu->eax = result;
    
    if (g_current_task_ptr) {
        if (jlos_syscall_need_resched()) {
            g_current_task_ptr->yield = false;
            return (uint32_t)jlos_task_manager_schedule(g_task_manager_ptr, (jlos_cpu_state_t *)cpu);
        }
    }
    return ctx;
}
