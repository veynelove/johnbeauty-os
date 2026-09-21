#include <hal/kernel_syscall.h>
#include <arch/x86/cpu_state.h>

static uint32_t s_x86_syscall_entry(void *handler, uint32_t ctx)
{
    (void)handler;
    jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)ctx;
    g_hal_syscall_trapframe = (jlos_cpu_state_t *)cpu;
    if (jlos_hal_syscall_dispatch) {
        cpu->eax = (uint32_t)jlos_hal_syscall_dispatch(cpu->eax, cpu->ebx, cpu->ecx, cpu->edx);
    }
    g_hal_syscall_trapframe = NULL;
    if (jlos_hal_syscall_resched_check && jlos_hal_syscall_resched_check()) {
        if (jlos_hal_syscall_resched_do) {
            return jlos_hal_syscall_resched_do(ctx);
        }
    }
    return ctx;
}

void jlos_hal_arch_syscall_init(void)
{
    jlos_hal_syscall_entry = s_x86_syscall_entry;
}