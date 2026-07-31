#ifndef JLOS_ARCH_X86_CPU_STATE_H
#define JLOS_ARCH_X86_CPU_STATE_H

#include <common/types.h>
#include <hal/cpu_state.h>

typedef struct
{
    uint32_t m_ebp;
    uint32_t m_edi;
    uint32_t m_esi;
    uint32_t m_edx;
    uint32_t m_ecx;
    uint32_t m_ebx;
    uint32_t m_eax;
    uint32_t m_error;
    uint32_t m_padding;
    uint32_t m_eip;
    uint32_t m_cs;
    uint32_t m_eflags;
    uint32_t m_user_esp;
    uint32_t m_user_ss;
} __attribute__((packed)) jlos_x86_regs_t;

_Static_assert(sizeof(jlos_cpu_state_t) >= sizeof(jlos_x86_regs_t), "cpu_state opaque too small for x86\n");

#endif
