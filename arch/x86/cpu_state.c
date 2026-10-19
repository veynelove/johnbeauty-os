/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/cpu_state.h>
#include <arch/x86/gdt.h>
#include <hal/cpu_state.h>

bool jlos_cpu_state_is_user_mode(jlos_cpu_state_t *s)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    return (r->cs & 3) != 0;
}

void jlos_cpu_state_init(jlos_cpu_state_t *s)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    jlos_memset(r, 0, sizeof(jlos_x86_regs_t));
    r->eflags = 0x200;
}

uint32_t jlos_cpu_state_get_syscall_num(jlos_cpu_state_t *s)
{
#ifdef CONFIG_X86_64
    return (uint32_t)((jlos_x86_regs_t *)s)->rax;
#else
    return ((jlos_x86_regs_t *)s)->eax;
#endif
}

void jlos_cpu_state_set_retval(jlos_cpu_state_t *s, int32_t val)
{
#ifdef CONFIG_X86_64
    ((jlos_x86_regs_t *)s)->rax = (uint64_t)(int64_t)val;
#else
    ((jlos_x86_regs_t *)s)->eax = val;
#endif
}

int32_t jlos_cpu_state_get_retval(jlos_cpu_state_t *s)
{
#ifdef CONFIG_X86_64
    return (int32_t)((jlos_x86_regs_t *)s)->rax;
#else
    return ((jlos_x86_regs_t *)s)->eax;
#endif
}

void jlos_cpu_state_set_user_entry(jlos_cpu_state_t *s, uintptr_t entry, uintptr_t stack_top)
{
    jlos_x86_regs_t *r = (jlos_x86_regs_t *)s;
    r->eip = entry;
    r->user_esp = stack_top;
    r->cs = JLOS_X86_USER_CS;
    r->user_ss = JLOS_X86_USER_DS;
    r->eflags = 0x0200;
}

void jlos_cpu_state_record_user_stack(jlos_cpu_state_t *curr_cpu, jlos_cpu_state_t *cpu)
{
    jlos_x86_regs_t *curr_c = (jlos_x86_regs_t *)curr_cpu;
    curr_c->user_esp = (uintptr_t)cpu + offsetof(jlos_x86_regs_t, user_esp);
    curr_c->user_ss  = 0;
}
