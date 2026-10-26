/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_X86_64

.text
.global jlos_irq_ignore_request
.global jlos_handle_interrupt_request0x00
.global jlos_handle_interrupt_request0x01
.global jlos_handle_interrupt_request0x02
.global jlos_handle_interrupt_request0x03
.global jlos_handle_interrupt_request0x04
.global jlos_handle_interrupt_request0x05
.global jlos_handle_interrupt_request0x06
.global jlos_handle_interrupt_request0x07
.global jlos_handle_interrupt_request0x08
.global jlos_handle_interrupt_request0x09
.global jlos_handle_interrupt_request0x0a
.global jlos_handle_interrupt_request0x0b
.global jlos_handle_interrupt_request0x0c
.global jlos_handle_interrupt_request0x0d
.global jlos_handle_interrupt_request0x0e
.global jlos_handle_interrupt_request0x0f
.global jlos_handle_interrupt_request0x31
.global jlos_handle_interrupt_request0x80
.global jlos_handle_interrupt_request0xfb
.global jlos_handle_interrupt_request0xfc
.global jlos_handle_interrupt_request0xef

.global jlos_handle_exception0x00
.global jlos_handle_exception0x01
.global jlos_handle_exception0x02
.global jlos_handle_exception0x03
.global jlos_handle_exception0x04
.global jlos_handle_exception0x05
.global jlos_handle_exception0x06
.global jlos_handle_exception0x07
.global jlos_handle_exception0x08
.global jlos_handle_exception0x09
.global jlos_handle_exception0x0a
.global jlos_handle_exception0x0b
.global jlos_handle_exception0x0c
.global jlos_handle_exception0x0d
.global jlos_handle_exception0x0e
.global jlos_handle_exception0x0f
.global jlos_handle_exception0x10
.global jlos_handle_exception0x11
.global jlos_handle_exception0x12
.global jlos_handle_exception0x13

/* 外部符号 */
.extern jlos_arch_tss_base_addr
.extern jlos_irq_manager_handle

.section .text

jlos_irq_ignore_request:
    cli
    pushl $0
    pushl $0xFF
    jmp int_bottom

.macro handle_interrupt_request num
    jlos_handle_interrupt_request\num:
        cli
        pushl $0
        pushl $\num
        jmp int_bottom
.endm

.macro handle_exception_no_err num
    jlos_handle_exception\num:
        cli
        pushl $0
        pushl $\num
        jmp error_frame
.endm

.macro handle_exception_has_err num
    jlos_handle_exception\num:
        cli
        pushl $\num
        jmp error_frame
.endm

handle_exception_no_err 0x00
handle_exception_no_err 0x01
handle_exception_no_err 0x02
handle_exception_no_err 0x03
handle_exception_no_err 0x04
handle_exception_no_err 0x05
handle_exception_no_err 0x06
handle_exception_no_err 0x07
handle_exception_has_err 0x08
handle_exception_no_err 0x09
handle_exception_has_err 0x0a
handle_exception_has_err 0x0b
handle_exception_has_err 0x0c
handle_exception_has_err 0x0d
handle_exception_has_err 0x0e
handle_exception_no_err 0x0f
handle_exception_no_err 0x10
handle_exception_no_err 0x11
handle_exception_no_err 0x12
handle_exception_no_err 0x13

handle_interrupt_request 0x00
handle_interrupt_request 0x01
handle_interrupt_request 0x02
handle_interrupt_request 0x03
handle_interrupt_request 0x04
handle_interrupt_request 0x05
handle_interrupt_request 0x06
handle_interrupt_request 0x07
handle_interrupt_request 0x08
handle_interrupt_request 0x09
handle_interrupt_request 0x0a
handle_interrupt_request 0x0b
handle_interrupt_request 0x0c
handle_interrupt_request 0x0d
handle_interrupt_request 0x0e
handle_interrupt_request 0x0f
handle_interrupt_request 0x31

handle_interrupt_request 0xfb
handle_interrupt_request 0xfc

handle_interrupt_request 0xef

/* 0x80 syscall handler: CPU switches to ring0 automatically (IDT uses kernel CS, DPL=3) */
/* Uses normal interrupt path - CPU pushes SS, ESP, EFLAGS, CS, EIP for ring3->ring0 switch */
jlos_handle_interrupt_request0x80:
    cli
    pushl $0
    pushl $0x80
    jmp int_bottom

int_bottom:
    testl $3, 12(%esp)
    jz ring0_no_err
    jmp ring3_no_err

ring0_no_err:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    jmp common_entry

ring3_no_err:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    movw $0x38, %ax
    movw %ax, %gs
    jmp common_entry

error_frame:
    testl $3, 12(%esp)
    jz ring0_err
    jmp ring3_err

ring0_err:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    jmp common_entry

ring3_err:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    movw $0x38, %ax
    movw %ax, %gs
    jmp common_entry

common_entry:
    movl 28(%esp), %eax     # eax = vector 字段(中断号), esp 此时指向寄存器帧
    pushl %esp              # arg2 = 寄存器帧指针
    pushl %eax              # arg1 = 中断号
    call jlos_irq_manager_handle

    cli                     # 调度器 / 中断处理可能开中断 (sti). 从现在到 iret
                            # 必须关中断保证原子性
    addl $8, %esp           # 清 2 个 arg; esp 指向本任务入栈的 regs

    testl $3, 40(%esp)      # cs 字段判 ring
    jnz ring3_finish

ring0_finish:
    popl %ebp
    popl %edi
    popl %esi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    addl $8, %esp           # 跳过 error + padding
    iret

ring3_finish:
    movw $0x2B, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    popl %ebp
    popl %edi
    popl %esi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    addl $8, %esp
    iret
.section .note.GNU-stack,"",%progbits
#else

/* ============ 64 位中断入口 ============ */
/* 流程: cli -> [pushq 占位(无错误码异常) 或 CPU 已压 err] -> pushq 向量 -> 判 ring
 * -> pushq 15 GPR -> (ring3 设 gs=percpu) -> call handler(SysV: rdi=vector rsi=regs)
 * -> cli -> popq 15 GPR -> addq $16 -> iretq
 * trapframe 64 位布局 (jlos_x86_regs_t): 15 GPR + vector + error + iretq frame(40B) = 176B */

.text
.global jlos_irq_ignore_request
.global jlos_handle_interrupt_request0x00
.global jlos_handle_interrupt_request0x01
.global jlos_handle_interrupt_request0x02
.global jlos_handle_interrupt_request0x03
.global jlos_handle_interrupt_request0x04
.global jlos_handle_interrupt_request0x05
.global jlos_handle_interrupt_request0x06
.global jlos_handle_interrupt_request0x07
.global jlos_handle_interrupt_request0x08
.global jlos_handle_interrupt_request0x09
.global jlos_handle_interrupt_request0x0a
.global jlos_handle_interrupt_request0x0b
.global jlos_handle_interrupt_request0x0c
.global jlos_handle_interrupt_request0x0d
.global jlos_handle_interrupt_request0x0e
.global jlos_handle_interrupt_request0x0f
.global jlos_handle_interrupt_request0x31
.global jlos_handle_interrupt_request0x80
.global jlos_handle_interrupt_request0xfb
.global jlos_handle_interrupt_request0xfc
.global jlos_handle_interrupt_request0xef

.global jlos_handle_exception0x00
.global jlos_handle_exception0x01
.global jlos_handle_exception0x02
.global jlos_handle_exception0x03
.global jlos_handle_exception0x04
.global jlos_handle_exception0x05
.global jlos_handle_exception0x06
.global jlos_handle_exception0x07
.global jlos_handle_exception0x08
.global jlos_handle_exception0x09
.global jlos_handle_exception0x0a
.global jlos_handle_exception0x0b
.global jlos_handle_exception0x0c
.global jlos_handle_exception0x0d
.global jlos_handle_exception0x0e
.global jlos_handle_exception0x0f
.global jlos_handle_exception0x10
.global jlos_handle_exception0x11
.global jlos_handle_exception0x12
.global jlos_handle_exception0x13

.extern jlos_irq_manager_handle

.section .text

/* 保存/恢复 15 个通用寄存器 */
.macro PUSH_GPRS64
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro POP_GPRS64
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
.endm

jlos_irq_ignore_request:
    cli
    pushq $0
    pushq $0xFF
    jmp int_bottom64

.macro handle_interrupt_request64 num
    jlos_handle_interrupt_request\num:
        cli
        pushq $0
        pushq $\num
        jmp int_bottom64
.endm

.macro handle_exception_no_err64 num
    jlos_handle_exception\num:
        cli
        pushq $0
        pushq $\num
        jmp error_frame64
.endm

.macro handle_exception_has_err64 num
    jlos_handle_exception\num:
        cli
        pushq $\num
        jmp error_frame64
.endm

handle_exception_no_err64 0x00
handle_exception_no_err64 0x01
handle_exception_no_err64 0x02
handle_exception_no_err64 0x03
handle_exception_no_err64 0x04
handle_exception_no_err64 0x05
handle_exception_no_err64 0x06
handle_exception_no_err64 0x07
handle_exception_has_err64 0x08
handle_exception_no_err64 0x09
handle_exception_has_err64 0x0a
handle_exception_has_err64 0x0b
handle_exception_has_err64 0x0c
handle_exception_has_err64 0x0d
handle_exception_has_err64 0x0e
handle_exception_no_err64 0x0f
handle_exception_no_err64 0x10
handle_exception_no_err64 0x11
handle_exception_no_err64 0x12
handle_exception_no_err64 0x13
handle_interrupt_request64 0x00
handle_interrupt_request64 0x01
handle_interrupt_request64 0x02
handle_interrupt_request64 0x03
handle_interrupt_request64 0x04
handle_interrupt_request64 0x05
handle_interrupt_request64 0x06
handle_interrupt_request64 0x07
handle_interrupt_request64 0x08
handle_interrupt_request64 0x09
handle_interrupt_request64 0x0a
handle_interrupt_request64 0x0b
handle_interrupt_request64 0x0c
handle_interrupt_request64 0x0d
handle_interrupt_request64 0x0e
handle_interrupt_request64 0x0f
handle_interrupt_request64 0x31
handle_interrupt_request64 0xfb
handle_interrupt_request64 0xfc
handle_interrupt_request64 0xef


/* 0x80 syscall: CPU 自动切换 ring0 (IDT 内核 CS, DPL=3), 压 40B iretq frame */
jlos_handle_interrupt_request0x80:
    cli
    pushq $0
    pushq $0x80
    jmp int_bottom64

/* 判 ring: cs 字段在 [vector(0)][err占位(8)][rip(16)][cs(24)] */
int_bottom64:

    testq $3, 24(%rsp)
    jz ring0_no_err64
    jmp ring3_no_err64

ring0_no_err64:
    PUSH_GPRS64
    jmp common_entry64

ring3_no_err64:
    PUSH_GPRS64

    jmp common_entry64

error_frame64:

    testq $3, 24(%rsp)
    jz ring0_err64
    jmp ring3_err64

ring0_err64:
    PUSH_GPRS64
    jmp common_entry64

ring3_err64:
    PUSH_GPRS64

    jmp common_entry64

/* vector 字段: 15 GPR (120B) 之后 */
common_entry64:
    movq 120(%rsp), %rdi    /* arg1 = vector (SysV rdi) */
    movq %rsp, %rsi         /* arg2 = regs 帧 (SysV rsi) */
    call jlos_irq_manager_handle

    cli                     /* 调度器可能开中断, 到 iretq 必须关中断 */
                            /* 寄存器传参无需清栈 */
    testq $3, 144(%rsp)     /* cs: 120(vector)+8(err)+8(eip) = 144 */
    jnz ring3_finish64

ring0_finish64:
    POP_GPRS64
    addq $16, %rsp          /* 跳过 vector + err 占位 */
    iretq

ring3_finish64:
    movw $0x2B, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs

    POP_GPRS64
    addq $16, %rsp
    iretq
#endif
.section .note.GNU-stack,"",%progbits
