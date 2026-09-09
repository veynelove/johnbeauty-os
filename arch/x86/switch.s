.text
.global jlos_hal_context_switch
.type   jlos_hal_context_switch, @function
jlos_hal_context_switch:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 20(%esp), %eax      # arg1 = old_sp_ptr
    movl 24(%esp), %edx      # arg2 = new_sp
    movl %esp, (%eax)        # *old_sp_ptr = 当前 esp（冻结本任务）
    movl %edx, %esp          # esp = new_sp（切到目标）
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret                      # 回到目标任务当初 call 的下一条
.size jlos_hal_context_switch, . - jlos_hal_context_switch
