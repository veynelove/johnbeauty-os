/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/signal.h>
#include <hal/syscall_abi.h>
#include <hal/hal_arch.h>
#include <arch/x86/cpu_state.h>

static const uint8_t s_sigreturn_code[8] = {
    0xb8, JLOS_SYSCALL_SIGRETURN, 0x00, 0x00, 0x00, 0xcd, 0x80, 0x90,
};

#define JLOS_SIGFRAME_SIG_OFFSET       sizeof(uintptr_t)
#define JLOS_SIGFRAME_REGS_OFFSET      (2 * sizeof(uintptr_t))
#define JLOS_SIGFRAME_SIGRETURN_OFFSET (JLOS_SIGFRAME_REGS_OFFSET + sizeof(jlos_x86_regs_t))
#define JLOS_SIGFRAME_SIZE             (JLOS_SIGFRAME_SIGRETURN_OFFSET + sizeof(s_sigreturn_code))

void jlos_arch_signal_frame_setup(jlos_cpu_state_t *tf, uint32_t sig, uintptr_t handler)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)tf;
    uintptr_t frame = r->user_esp - JLOS_SIGFRAME_SIZE;
    jlos_hal_uaccess_begin();
    *(uintptr_t *)(frame) = frame + JLOS_SIGFRAME_SIGRETURN_OFFSET;
    *(uintptr_t *)(frame + JLOS_SIGFRAME_SIG_OFFSET) = sig;

    jlos_memcpy((void *)(frame + JLOS_SIGFRAME_REGS_OFFSET), r, sizeof(jlos_x86_regs_t));
    jlos_memcpy((void *)(frame + JLOS_SIGFRAME_SIGRETURN_OFFSET), s_sigreturn_code, sizeof(s_sigreturn_code));
    jlos_hal_uaccess_end();
    r->user_esp = frame;
    r->eip = handler;
#ifdef CONFIG_X86_64
    r->rdi = sig;
#endif
}

void jlos_arch_signal_frame_restore(jlos_cpu_state_t *tf)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)tf;
    jlos_x86_regs_t saved;
    jlos_hal_uaccess_begin();
    jlos_memcpy(&saved, (const void *)(r->user_esp + JLOS_SIGFRAME_SIG_OFFSET), sizeof(saved));
    jlos_hal_uaccess_end();
    *r = saved;
}
