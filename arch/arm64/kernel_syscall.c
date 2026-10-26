/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <common/types.h>
#include <hal/kernel_syscall.h>
#include <arch/arm64/interrupts.h>
#include <kernel/multitask.h>

static uintptr_t s_arm64_syscall_entry(void *handler, uintptr_t ctx)
{
    (void)handler;
    jlos_task_t *me = jlos_hal_current_task();
    jlos_arm64_pt_regs_t *frame = (jlos_arm64_pt_regs_t *)ctx;
    if (me) {
        me->syscall_tf = (jlos_cpu_state_t *)frame;
    }
    if (jlos_hal_syscall_dispatch) {
        frame->regs[0] = (uint64_t)(int64_t)(int32_t)jlos_hal_syscall_dispatch(
            (uint32_t)frame->regs[8], frame->regs[0], frame->regs[1], frame->regs[2]);
    }
    if (me) {
        me->syscall_tf = nullptr;
    }
    if (jlos_hal_syscall_resched_check && jlos_hal_syscall_resched_check()) {
        if (jlos_hal_syscall_resched_do) {
            ctx = jlos_hal_syscall_resched_do(ctx);
        }
    }
    if (me) {
        jlos_signal_check_deliver(me, (jlos_cpu_state_t *)ctx);
    }
    return ctx;
}

void jlos_hal_arch_syscall_init(void)
{
    jlos_hal_syscall_entry = s_arm64_syscall_entry;
}
