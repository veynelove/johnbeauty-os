#include <hal/signal.h>
#include <hal/syscall_abi.h>
#include <arch/x86/cpu_state.h>

static const uint8_t s_sigreturn_code[8] = {
    0xb8, JLOS_SYSCALL_SIGRETURN, 0x00, 0x00, 0x00, 0xcd, 0x80, 0x90,
};

void jlos_arch_signal_frame_setup(jlos_cpu_state_t *tf, uint32_t sig, uint32_t handler)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)tf;
    uint32_t frame = r->user_esp - 72;
    *(uint32_t *)(frame) = frame + 64;
    *(uint32_t *)(frame + 4) = sig;

    jlos_memcpy((void *)(frame + 8), r, sizeof(jlos_x86_regs_t));
    jlos_memcpy((void *)(frame + 64), s_sigreturn_code, 8);
    r->user_esp = frame;
    r->eip = handler;
}

void jlos_arch_signal_frame_restore(jlos_cpu_state_t *tf)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)tf;
    jlos_x86_regs_t saved;
    jlos_memcpy(&saved, (const void *)(r->user_esp + 4), sizeof(saved));
    *r = saved;
}
