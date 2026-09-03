#ifndef _JLOS_ARCH_X86_CPU_STATE_H
#define _JLOS_ARCH_X86_CPU_STATE_H

#include <common/types.h>
#include <hal/cpu_state.h>

typedef struct
{
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint32_t error;
    uint32_t padding;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t user_esp;
    uint32_t user_ss;
} __attribute__((packed)) jlos_x86_regs_t;

_Static_assert(sizeof(jlos_cpu_state_t) >= sizeof(jlos_x86_regs_t), "cpu_state opaque too small for x86\n");

#endif
