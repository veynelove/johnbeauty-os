/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

.text
.global jlos_memcpy
.type jlos_memcpy, %function
jlos_memcpy:
    mov     x3, x0
1:
    cmp     x2, #16
    b.lo    2f
    ldp     x4, x5, [x1], #16
    stp     x4, x5, [x0], #16
    sub     x2, x2, #16
    b       1b
2:
    cbz     x2, 3f
4:
    ldrb    w4, [x1], #1
    strb    w4, [x0], #1
    subs    x2, x2, #1
    b.ne    4b
3:
    mov     x0, x3
    ret
.size jlos_memcpy, .-jlos_memcpy

.section .note.GNU-stack,"",%progbits