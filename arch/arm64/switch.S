/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

/* callee-saved + SP 保存/切换；ret 落到新任务栈顶预置的 stub 地址 */
.text
.global jlos_hal_context_switch
.type   jlos_hal_context_switch, @function
jlos_hal_context_switch:
    stp     x29, x30, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x19, x20, [sp, #-16]!
    mov     x9, sp
    str     x9, [x0]
    mov     sp, x1
    ldp     x19, x20, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x29, x30, [sp], #16
    ret

.section .note.GNU-stack,"",%progbits
