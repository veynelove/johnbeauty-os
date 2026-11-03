/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

.text
.global jlos_hal_context_switch
.type   jlos_hal_context_switch, @function
jlos_hal_context_switch:
#ifdef CONFIG_X86_64
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, (%rdi)        /* *old_sp_ptr = 当前 rsp */
    movq %rsi, %rsp          /* rsp = new_sp */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
#else
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 20(%esp), %eax      /* arg1 = old_sp_ptr */
    movl 24(%esp), %edx      /* arg2 = new_sp */
    movl %esp, (%eax)        /* *old_sp_ptr = 当前 esp */
    movl %edx, %esp          /* esp = new_sp */
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
#endif
.size jlos_hal_context_switch, . - jlos_hal_context_switch
.section .note.GNU-stack,"",%progbits
