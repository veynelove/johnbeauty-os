/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/signal.h>
#include <hal/hal_arch.h>
#include <hal/syscall_abi.h>
#include <arch/arm64/interrupts.h>
#include <arch/arm64/pt_regs.h>

static const uint32_t s_sigreturn_code[] = {
    0xd2800008 | (JLOS_SYSCALL_SIGRETURN << 5),
    0xd4000001,
};

#define JLOS_ARM64_SIGFRAME_TRAMP_OFFSET   JLOS_ARM64_PT_SIZE
#define JLOS_ARM64_SIGFRAME_TRAMP_SIZE     sizeof(s_sigreturn_code)
#define JLOS_ARM64_SIGFRAME_SIZE           (JLOS_ARM64_SIGFRAME_TRAMP_OFFSET + JLOS_ARM64_SIGFRAME_TRAMP_SIZE)

void jlos_arch_signal_frame_setup(jlos_cpu_state_t *tf, uint32_t sig, uintptr_t handler)
{
    jlos_arm64_pt_regs_t *r = (jlos_arm64_pt_regs_t *)tf;
    uintptr_t frame = (r->sp - JLOS_ARM64_SIGFRAME_SIZE) & ~(uintptr_t)15;
    jlos_hal_uaccess_begin();
    jlos_memcpy((void *)frame, r, JLOS_ARM64_PT_SIZE);
    jlos_memcpy((void *)(frame + JLOS_ARM64_SIGFRAME_TRAMP_OFFSET), s_sigreturn_code, JLOS_ARM64_SIGFRAME_TRAMP_SIZE);
    jlos_hal_uaccess_end();
    r->sp = frame;
    r->pc = handler;
    r->regs[0] = sig;
    r->regs[30] = frame + JLOS_ARM64_SIGFRAME_TRAMP_OFFSET;
}

void jlos_arch_signal_frame_restore(jlos_cpu_state_t *tf)
{
    jlos_arm64_pt_regs_t *r = (jlos_arm64_pt_regs_t *)tf;
    jlos_arm64_pt_regs_t saved;
    jlos_hal_uaccess_begin();
    jlos_memcpy(&saved, (const void *)r->sp, JLOS_ARM64_PT_SIZE);
    jlos_hal_uaccess_end();
    *r = saved;
}
