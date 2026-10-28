/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

.text
.global jlos_hal_raw_copy_from_user
.global jlos_hal_raw_copy_to_user

jlos_hal_raw_copy_from_user:
    mov x3, x0
    cbz x2, .Lcfu_success
.Lcfu_loop:
.Lcfu_read:
    ldrb w4, [x1]
.Lcfu_write:
    strb w4, [x3]
    add x1, x1, #1
    add x3, x3, #1
    subs x2, x2, #1
    b.ne .Lcfu_loop
.Lcfu_success:
    mov x0, #0
    ret
.Lcfu_fault:
    mov x0, #1
    ret

jlos_hal_raw_copy_to_user:
    mov x3, x0
    cbz x2, .Lctu_success
.Lctu_loop:
.Lctu_read:
    ldrb w4, [x1]
.Lctu_write:
    strb w4, [x3]
    add x1, x1, #1
    add x3, x3, #1
    subs x2, x2, #1
    b.ne .Lctu_loop
.Lctu_success:
    mov x0, #0
    ret
.Lctu_fault:
    mov x0, #1
    ret

.pushsection .ex_table, "aw"
    .balign 8
    .quad .Lcfu_read
    .quad .Lcfu_fault
    .quad .Lcfu_write
    .quad .Lcfu_fault
    .quad .Lctu_read
    .quad .Lctu_fault
    .quad .Lctu_write
    .quad .Lctu_fault
.popsection

.section .note.GNU-stack,"",%progbits
