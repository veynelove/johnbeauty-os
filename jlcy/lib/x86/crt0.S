#define __ASSEMBLY__
#include <include/syscall_abi.h>

.section .text
.global _start
_start:
    movl    (%esp), %eax
    leal    4(%esp), %edx
    pushl   %edx
    pushl   %eax
    call    main
    movl    %eax, %ebx
    movl    $JLOS_SYSCALL_EXIT, %eax
    int     $0x80
1:  hlt
    jmp     1b
.section .note.GNU-stack,"",%progbits
