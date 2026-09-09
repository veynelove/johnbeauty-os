.text
.global jlos_ignore_interrupt_request
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

# 外部符号
.extern jlos_arch_tss_base_addr
.extern jlos_interrupt_manager_handle_interrupt

.global interruptnumber
interruptnumber:
    .long 0

.section .text

jlos_ignore_interrupt_request:
    cli
    movl $0xFF, (interruptnumber)
    pushl $0
    jmp int_bottom

.macro handle_interrupt_request num
    jlos_handle_interrupt_request\num:
        cli
        movl $\num, (interruptnumber)
        pushl $0
        jmp int_bottom
.endm

.macro handle_exception_no_err num
    jlos_handle_exception\num:
        cli
        movl $\num, (interruptnumber)
        pushl $0
        jmp error_frame
.endm

.macro handle_exception_has_err num
    jlos_handle_exception\num:
        cli
        movl $\num, (interruptnumber)
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

# 0x80 syscall handler: CPU switches to ring0 automatically (IDT uses kernel CS, DPL=3)
# Uses normal interrupt path - CPU pushes SS, ESP, EFLAGS, CS, EIP for ring3->ring0 switch
jlos_handle_interrupt_request0x80:
    cli
    movl $0x80, (interruptnumber)
    pushl $0
    jmp int_bottom

int_bottom:
    testl $3, 8(%esp)
    jz ring0_no_err
    jmp ring3_no_err

ring0_no_err:
    pushl $0
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    jmp common_entry

ring3_no_err:
    pushl $0
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    jmp common_entry

error_frame:
    testl $3, 8(%esp)
    jz ring0_err
    jmp ring3_err

ring0_err:
    pushl $0
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    jmp common_entry

ring3_err:
    pushl $0
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi
    pushl %ebp
    jmp common_entry

common_entry:
    pushl %esp
    pushl (interruptnumber)
    call jlos_interrupt_manager_handle_interrupt

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
