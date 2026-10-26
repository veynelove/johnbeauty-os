/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#define __ASSEMBLY__
#include <include/syscall_abi.h>

.section .text
.global _start
_start:
    mov     w8, #SYSCALL_WRITE
    mov     x0, #1
    adr     x1, .Lu
    mov     x2, #1
    svc     #0
    ldr     x0, [sp]
    add     x1, sp, #8
    bl      main
    mov     w8, #SYSCALL_EXIT
    svc     #0
1:  wfe
    b       1b
.Lu:
    .ascii "u"
.section .note.GNU-stack,"",%progbits
