/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <common/types.h>
#include <arch/arm64/interrupts.h>
#include <arch/arm64/pt_regs.h>
#include <hal/cpu_state.h>

bool jlos_cpu_state_is_user_mode(jlos_cpu_state_t *s)
{
    return (((jlos_arm64_pt_regs_t *)s)->pstate & 0x1F) == JLOS_ARM64_SPSR_EL0T;
}

void jlos_cpu_state_init(jlos_cpu_state_t *s)
{
    jlos_memset(s, 0, sizeof(*s));
}

uint32_t jlos_cpu_state_get_syscall_num(jlos_cpu_state_t *s)
{
    return (uint32_t)((jlos_arm64_pt_regs_t *)s)->regs[8];
}

void jlos_cpu_state_set_retval(jlos_cpu_state_t *s, int32_t val)
{
    ((jlos_arm64_pt_regs_t *)s)->regs[0] = (uint64_t)(int64_t)val;
}

int32_t jlos_cpu_state_get_retval(jlos_cpu_state_t *s)
{
    return (int32_t)((jlos_arm64_pt_regs_t *)s)->regs[0];
}

void jlos_cpu_state_set_user_entry(jlos_cpu_state_t *s, uintptr_t entry, uintptr_t stack_top)
{
    jlos_arm64_pt_regs_t *r = (jlos_arm64_pt_regs_t *)s;
    r->pc = entry;
    r->sp = stack_top;
    r->pstate = JLOS_ARM64_SPSR_EL0T;
}

void jlos_cpu_state_record_user_stack(jlos_cpu_state_t *curr_cpu, jlos_cpu_state_t *cpu)
{
    ((jlos_arm64_pt_regs_t *)curr_cpu)->sp = ((jlos_arm64_pt_regs_t *)cpu)->sp;
}
