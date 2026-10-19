/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/kernel_syscall.h>
#include <arch/x86/cpu_state.h>
#include <kernel/multitask.h>

static uintptr_t s_x86_syscall_entry(void *handler, uintptr_t ctx)
{
    (void)handler;
    jlos_task_t *current_task = jlos_hal_current_task();
    jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)ctx;
    jlos_task_t *me = current_task;
    if (me) {
        me->syscall_tf = (jlos_cpu_state_t *)cpu;
    }
    if (jlos_hal_syscall_dispatch) {
#ifdef CONFIG_X86_64
        cpu->rax = (uint64_t)(int64_t)(int32_t)jlos_hal_syscall_dispatch(
            (uint32_t)cpu->rax, (uint32_t)cpu->rbx, (uint32_t)cpu->rcx, (uint32_t)cpu->rdx);
#else
        cpu->eax = (uint32_t)jlos_hal_syscall_dispatch(cpu->eax, cpu->ebx, cpu->ecx, cpu->edx);
#endif
    }
    if (me) {
        me->syscall_tf = nullptr;
    }
    if (jlos_hal_syscall_resched_check && jlos_hal_syscall_resched_check()) {
        if (jlos_hal_syscall_resched_do) {
            ctx = jlos_hal_syscall_resched_do(ctx);
        }
    }
    jlos_task_t *curr = current_task;
    if (curr) {
        jlos_signal_check_deliver(curr, (jlos_cpu_state_t *)ctx);
    }
    return ctx;
}

void jlos_hal_arch_syscall_init(void)
{
    jlos_hal_syscall_entry = s_x86_syscall_entry;
}