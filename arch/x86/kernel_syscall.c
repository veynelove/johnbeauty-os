#include <hal/kernel_syscall.h>
#include <arch/x86/cpu_state.h>
#include <kernel/multitask.h>

static uint32_t s_x86_syscall_entry(void *handler, uint32_t ctx)
{
    (void)handler;
    jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)ctx;
    jlos_task_t *me = g_current_task_ptr;
    if (me) {
        me->syscall_tf = (jlos_cpu_state_t *)cpu;
    }
    if (jlos_hal_syscall_dispatch) {
        cpu->eax = (uint32_t)jlos_hal_syscall_dispatch(cpu->eax, cpu->ebx, cpu->ecx, cpu->edx);
    }
    if (me) {
        me->syscall_tf = NULL;
    }
    if (jlos_hal_syscall_resched_check && jlos_hal_syscall_resched_check()) {
        if (jlos_hal_syscall_resched_do) {
            ctx = jlos_hal_syscall_resched_do(ctx);
        }
    }
    jlos_task_t *curr = g_current_task_ptr;
    if (curr) {
        jlos_signal_check_deliver(curr, (jlos_cpu_state_t *)ctx);
    }
    return ctx;
}

void jlos_hal_arch_syscall_init(void)
{
    jlos_hal_syscall_entry = s_x86_syscall_entry;
}