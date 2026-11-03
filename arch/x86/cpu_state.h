/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_CPU_STATE_H
#define _JLOS_ARCH_X86_CPU_STATE_H

#include <common/types.h>
#include <hal/cpu_state.h>

#ifdef CONFIG_X86_64
typedef struct
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error;
    uint64_t eip;
    uint64_t cs;
    uint64_t eflags;
    uint64_t user_esp;
    uint64_t user_ss;
} __attribute__((packed)) jlos_x86_regs_t;
#else
typedef struct
{
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
    uint32_t vector;
    uint32_t error;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t user_esp;
    uint32_t user_ss;
} __attribute__((packed)) jlos_x86_regs_t;
#endif

static_assert(sizeof(jlos_cpu_state_t) >= sizeof(jlos_x86_regs_t), "cpu_state opaque too small for x86\n");

#endif
