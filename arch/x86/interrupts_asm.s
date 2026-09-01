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

# 全局变量：保存原始栈指针，用于 ring3 返回路径
.section .bss
.global jlos_ring3_original_esp
jlos_ring3_original_esp:
    .long 0

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
                            # 必须关中断保证原子性:
                            #   1) esp = new cpustate 基 (L197) 之后, 如果 IF=1,
                            #      IRQ 插进来 pushal 会把 new cpustate.GPR (HAL 写的
                            #      eax=0/ebx=retpc/eip=ret_from_fork) 瞬间覆写成
                            #      随机上下文 → pid=4 c≠NULL / pid=6 jmp 0xC3C910C4
                            #      这类错位崩溃 100% 命中.
                            #   2) .stack_return 无切换同理, 从 eax/ecx 到 iret
                            #      期间不能被改写.
                            # iret 通过弹 EFLAGS.IF=1 (父保存的原值) 自动重新开中断.
    movl 4(%esp), %ecx
    cmpl %eax, %ecx
    je .stack_return

    movl %eax, %esp
    testl $3, 40(%esp)
    jnz ring3_return
    jmp ring0_task_return

.stack_return:
    addl $8, %esp
    testl $3, 40(%esp)
    jnz ring3_return

    popl %ebp
    popl %edi
    popl %esi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    addl $8, %esp
    iret

ring0_task_return:
    # esp = NEW cpustate 基 (sched 返回 &next->cpustate 后 common_entry L197
    # movl %eax, %esp 已切换). 恢复寄存器 & iret frame 全部从 NEW cpustate
    # 读, 不再用 old cpustate (此前 edi=jlos_ring3_original_esp=old 的 bug).
    #   OFFSET 0-27: ebp,edi,esi,edx,ecx,ebx,eax (7 GPR)
    #   OFFSET 28-35: error_code + int_no
    #   OFFSET 36/40/44: eip / cs / eflags (iret frame 3 x 4B)
    #   OFFSET 48/52: user_esp / user_ss
    movl %esp, %edi              # edi = NEW cpustate 基指针 (不再用 old!)
    movl 48(%edi), %ecx          # ecx = NEW.user_esp (任务内核栈指针)
    movl %ecx, %esp              # 切到任务内核栈 (此时 esp=user_esp)

    movl 44(%edi), %ecx          # 3 x pushl 构造 iret frame, 内容来自 NEW cpustate
    pushl %ecx
    movl 40(%edi), %ecx
    pushl %ecx
    movl 36(%edi), %ecx
    pushl %ecx

    movl 24(%edi), %eax          # 恢复 7 GPR, 全来自 NEW cpustate
    movl 20(%edi), %ebx          # ebx = NEW.ebx -> HAL 写 fork_retpc 在这里生效
    movl 16(%edi), %ecx
    movl 12(%edi), %edx
    movl  8(%edi), %esi
    movl  0(%edi), %ebp
    movl  4(%edi), %edi          # 最后一步写 edi, 不再需要源基指针

    iret                         # 弹出 eip/cs/eflags -> CPU 寄存器 = NEW cpustate

ring3_return:
    # esp = NEW cpustate 基 (与 ring0_task_return 同前提). 基址改为 NEW
    movl %esp, %edi              # edi = NEW cpustate 基
    # ring3 iret 从 TSS.esp0 拿 ring0 栈, TSS 已在 task user init 时设好,
    # 直接用.  esp 切到 TSS 后 new cpustate 指针 edi 还留着.
    movl (jlos_arch_tss_base_addr), %ecx
    movl 4(%ecx), %ecx
    movl %ecx, %esp

    movl 52(%edi), %ecx          # 5 x pushl = user_ss/user_esp/eflags/cs/eip
    pushl %ecx
    movl 48(%edi), %ecx
    pushl %ecx
    movl 44(%edi), %ecx
    pushl %ecx
    movl 40(%edi), %ecx
    pushl %ecx
    movl 36(%edi), %ecx
    pushl %ecx

    movw $0x2B, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movl 24(%edi), %eax          # 恢复 GPR, 全来自 NEW cpustate
    movl 20(%edi), %ebx
    movl 16(%edi), %ecx
    movl 12(%edi), %edx
    movl  8(%edi), %esi
    movl  0(%edi), %ebp
    movl  4(%edi), %edi

    iret

# ============================================================================
# ret_from_fork - fork 子任务首次 iret 后的专用跳板 (经典 Linux copy_thread)
# ============================================================================
.global ret_from_fork
.type ret_from_fork, @function
ret_from_fork:
    movl $0, %eax               # eax=0 -> fork 子 retval == NULL (最终防线)
    jmp  *%ebx                  # 不经过 fork 尾部 return child, 直接跳回 caller
.size ret_from_fork, . - ret_from_fork

