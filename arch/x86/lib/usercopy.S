/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

.text
.global jlos_hal_raw_copy_from_user
.global jlos_hal_raw_copy_to_user

#ifdef CONFIG_X86_64
jlos_hal_raw_copy_from_user:
    movq %rdx, %rcx
    testq %rcx, %rcx
    jz .Lcfu_success
.Lcfu_loop:
.Lcfu_read:
    movb (%rsi), %al
.Lcfu_write:
    movb %al, (%rdi)
    incq %rsi
    incq %rdi
    decq %rcx
    jnz .Lcfu_loop
.Lcfu_success:
    xorl %eax, %eax
    ret
.Lcfu_fault:
    movl $1, %eax
    ret

jlos_hal_raw_copy_to_user:
    movq %rdx, %rcx
    testq %rcx, %rcx
    jz .Lctu_success
.Lctu_loop:
.Lctu_read:
    movb (%rsi), %al
.Lctu_write:
    movb %al, (%rdi)
    incq %rsi
    incq %rdi
    decq %rcx
    jnz .Lctu_loop
.Lctu_success:
    xorl %eax, %eax
    ret
.Lctu_fault:
    movl $1, %eax
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
#else
jlos_hal_raw_copy_from_user:
    pushl %edi
    pushl %esi
    movl 12(%esp), %edi
    movl 16(%esp), %esi
    movl 20(%esp), %ecx
    testl %ecx, %ecx
    jz .Lcfu_success
.Lcfu_loop:
.Lcfu_read:
    movb (%esi), %al
.Lcfu_write:
    movb %al, (%edi)
    incl %esi
    incl %edi
    decl %ecx
    jnz .Lcfu_loop
.Lcfu_success:
    xorl %eax, %eax
    popl %esi
    popl %edi
    ret
.Lcfu_fault:
    movl $1, %eax
    popl %esi
    popl %edi
    ret

jlos_hal_raw_copy_to_user:
    pushl %edi
    pushl %esi
    movl 12(%esp), %edi
    movl 16(%esp), %esi
    movl 20(%esp), %ecx
    testl %ecx, %ecx
    jz .Lctu_success
.Lctu_loop:
.Lctu_read:
    movb (%esi), %al
.Lctu_write:
    movb %al, (%edi)
    incl %esi
    incl %edi
    decl %ecx
    jnz .Lctu_loop
.Lctu_success:
    xorl %eax, %eax
    popl %esi
    popl %edi
    ret
.Lctu_fault:
    movl $1, %eax
    popl %esi
    popl %edi
    ret

.pushsection .ex_table, "aw"
    .balign 4
    .long .Lcfu_read
    .long .Lcfu_fault
    .long .Lcfu_write
    .long .Lcfu_fault
    .long .Lctu_read
    .long .Lctu_fault
    .long .Lctu_write
    .long .Lctu_fault
.popsection
#endif

.section .note.GNU-stack,"",%progbits
