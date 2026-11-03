/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

.text
.global jlos_memset
.type jlos_memset, %function
jlos_memset:
    and     x1, x1, #0xff
    ldr     x4, =0x0101010101010101
    mul     x1, x1, x4
1:
    cmp     x2, #16
    b.lo    2f
    stp     x1, x1, [x0], #16
    sub     x2, x2, #16
    b       1b
2:
    cbz     x2, 3f
4:
    strb    w1, [x0], #1
    subs    x2, x2, #1
    b.ne    4b
3:
    ret
.size jlos_memset, .-jlos_memset

.section .note.GNU-stack,"",%progbits