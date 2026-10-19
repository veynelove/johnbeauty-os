/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#define __ASSEMBLY__
#include <include/syscall_abi.h>

.section .text
.global _start
_start:
#ifdef __x86_64__
    /* 输出 "u" 标记确认用户程序开始执行 */
    movl    $SYSCALL_WRITE, %eax
    movl    $1, %ebx
    leaq    .Lu(%rip), %rcx
    movl    $1, %edx
    int     $0x80
    /* SysV ABI: rdi=argc, rsi=argv */
    movq    (%rsp), %rdi
    leaq    8(%rsp), %rsi
    call    main
    movq    %rax, %rbx
    movl    $SYSCALL_EXIT, %eax
    int     $0x80
#else
    movl    (%esp), %eax
    leal    4(%esp), %edx
    pushl   %edx
    pushl   %eax
    call    main
    movl    %eax, %ebx
    movl    $SYSCALL_EXIT, %eax
    int     $0x80
#endif
1:  hlt
    jmp     1b
.Lu:
    .ascii "u"
.section .note.GNU-stack,"",%progbits
