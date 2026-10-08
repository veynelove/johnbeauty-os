/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_TRAMPOLINE_H
#define _JLOS_ARCH_X86_TRAMPOLINE_H

#include <common/types.h>

/* AP 实模式起始物理地址 (页对齐); SIPI vector = 此值 >> 12。
 * 必须与 trampoline.s 里的 0x8000 保持一致。 */
#define JLOS_X86_AP_TRAMPOLINE_PHYS     0x8000
#define JLOS_X86_AP_TRAMPOLINE_VECTOR   (JLOS_X86_AP_TRAMPOLINE_PHYS >> 12)

extern uint8_t  jlos_x86_trampoline_start[];
extern uint8_t  jlos_x86_trampoline_end[];
extern uint8_t  jlos_x86_trampoline_gdt_desc[];
extern uint8_t  jlos_x86_trampoline_gdt[];
extern uint8_t  jlos_x86_trampoline_prot32[];
extern uint8_t  jlos_x86_trampoline_far_ptr[];
extern uint8_t  jlos_x86_trampoline_high_ptr[];
extern uint8_t  jlos_x86_trampoline_cr4[];
extern uint8_t  jlos_x86_trampoline_cr3[];
extern uint8_t  jlos_x86_trampoline_stack[];

#endif
