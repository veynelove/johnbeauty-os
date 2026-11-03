/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/pt_regs.h>

.macro IRQ_VECTOR
    b       .L_irq_common
    .space  (128 - 4)
.endm

.macro SYNC_VECTOR
    b       .L_sync_common
    .space  (128 - 4)
.endm

.macro BAD_VECTOR kind
    mov     x0, \kind
    b       jlos_arm64_bad_dispatch
    .space  (128 - 8)
.endm

/* 保存全部通用寄存器 + 异常返回状态，x0 传帧指针后可安全进 C */
.macro SAVE_FRAME
    sub     sp, sp, #JLOS_ARM64_PT_SIZE
    stp     x0, x1,   [sp, #JLOS_ARM64_PT_X(0)]
    stp     x2, x3,   [sp, #JLOS_ARM64_PT_X(2)]
    stp     x4, x5,   [sp, #JLOS_ARM64_PT_X(4)]
    stp     x6, x7,   [sp, #JLOS_ARM64_PT_X(6)]
    stp     x8, x9,   [sp, #JLOS_ARM64_PT_X(8)]
    stp     x10, x11, [sp, #JLOS_ARM64_PT_X(10)]
    stp     x12, x13, [sp, #JLOS_ARM64_PT_X(12)]
    stp     x14, x15, [sp, #JLOS_ARM64_PT_X(14)]
    stp     x16, x17, [sp, #JLOS_ARM64_PT_X(16)]
    stp     x18, x19, [sp, #JLOS_ARM64_PT_X(18)]
    stp     x20, x21, [sp, #JLOS_ARM64_PT_X(20)]
    stp     x22, x23, [sp, #JLOS_ARM64_PT_X(22)]
    stp     x24, x25, [sp, #JLOS_ARM64_PT_X(24)]
    stp     x26, x27, [sp, #JLOS_ARM64_PT_X(26)]
    stp     x28, x29, [sp, #JLOS_ARM64_PT_X(28)]
    str     x30,      [sp, #JLOS_ARM64_PT_X(30)]
    mrs     x0, sp_el0
    str     x0, [sp, #JLOS_ARM64_PT_SP_OFF]
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x0, x1, [sp, #JLOS_ARM64_PT_PC_OFF]
.endm

.macro RESTORE_FRAME
    ldp     x0, x1, [sp, #JLOS_ARM64_PT_PC_OFF]
    msr     elr_el1, x0
    msr     spsr_el1, x1
    /* sp_el0: 用户 SP 独立于 SP_EL1，调度切换后必须从帧恢复 */
    ldr     x0, [sp, #JLOS_ARM64_PT_SP_OFF]
    msr     sp_el0, x0
    ldp     x28, x29, [sp, #JLOS_ARM64_PT_X(28)]
    ldr     x30,      [sp, #JLOS_ARM64_PT_X(30)]
    ldp     x26, x27, [sp, #JLOS_ARM64_PT_X(26)]
    ldp     x24, x25, [sp, #JLOS_ARM64_PT_X(24)]
    ldp     x22, x23, [sp, #JLOS_ARM64_PT_X(22)]
    ldp     x20, x21, [sp, #JLOS_ARM64_PT_X(20)]
    ldp     x18, x19, [sp, #JLOS_ARM64_PT_X(18)]
    ldp     x16, x17, [sp, #JLOS_ARM64_PT_X(16)]
    ldp     x14, x15, [sp, #JLOS_ARM64_PT_X(14)]
    ldp     x12, x13, [sp, #JLOS_ARM64_PT_X(12)]
    ldp     x10, x11, [sp, #JLOS_ARM64_PT_X(10)]
    ldp     x8, x9,   [sp, #JLOS_ARM64_PT_X(8)]
    ldp     x6, x7,   [sp, #JLOS_ARM64_PT_X(6)]
    ldp     x4, x5,   [sp, #JLOS_ARM64_PT_X(4)]
    ldp     x2, x3,   [sp, #JLOS_ARM64_PT_X(2)]
    ldp     x0, x1,   [sp, #JLOS_ARM64_PT_X(0)]
    add     sp, sp, #JLOS_ARM64_PT_SIZE
    eret
.endm

.section .vectors, "ax"
.balign 2048
.global vectors
vectors:
    /* current EL, SP_EL0 — SPSel=1 内核不触发，防御性 panic */
    BAD_VECTOR 0
    BAD_VECTOR 1
    BAD_VECTOR 2
    BAD_VECTOR 3
    /* current EL, SP_ELx — 内核态 */
    SYNC_VECTOR                    /* 0x200 */
    IRQ_VECTOR                     /* 0x280 */
    BAD_VECTOR 6
    BAD_VECTOR 7
    /* lower EL, AArch64 — 用户态 */
    SYNC_VECTOR                    /* 0x400 */
    IRQ_VECTOR                     /* 0x480 */
    BAD_VECTOR 10
    BAD_VECTOR 11
    /* lower EL, AArch32 */
    SYNC_VECTOR
    IRQ_VECTOR
    BAD_VECTOR 14
    BAD_VECTOR 15

.L_irq_common:
    SAVE_FRAME
    mov     x0, sp
    bl      jlos_arm64_irq_dispatch
    RESTORE_FRAME

.L_sync_common:
    SAVE_FRAME
    /* ESR/FAR 立即入帧：防止处理路径上的嵌套异常覆盖 */
    mrs     x0, esr_el1
    mrs     x1, far_el1
    stp     x0, x1, [sp, #JLOS_ARM64_PT_ESR_OFF]
    mov     x0, sp
    bl      jlos_arm64_sync_dispatch
    RESTORE_FRAME

.section .text

/* 内核任务首入：x19 = entrypoint（context_switch 恢复序列载入） */
.global jlos_task_entry_stub
jlos_task_entry_stub:
    mov     x29, xzr
    msr     daifclr, #2
    blr     x19
    b       jlos_task_do_exit

/* 用户任务首入：x20 = 用户 pc, x21 = 用户 sp */
.global jlos_task_user_entry_stub
jlos_task_user_entry_stub:
    msr     elr_el1, x20
    msr     spsr_el1, xzr
    msr     sp_el0, x21
    eret

/* fork 子任务首入：x19 = 预置 trapframe 基址 */
.global jlos_task_ret_from_fork_stub
jlos_task_ret_from_fork_stub:
    mov     sp, x19
    RESTORE_FRAME

.section .note.GNU-stack,"",%progbits
