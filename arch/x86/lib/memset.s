# SSE2 优化 memset — 32B 宽写
# void jlos_memset(void *ptr, uint8_t value, size_t size)
.text
.global jlos_memset
jlos_memset:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi

    movl 8(%ebp), %esi        # ptr
    movzbl 12(%ebp), %eax     # value
    movl 16(%ebp), %edx       # size

    cmpl $64, %edx
    jb .Lmemset_byte

    # 检查 CR4.OSFXSR (bit 9) 是否已设置
    movl %cr4, %ecx
    testl $0x200, %ecx
    jz .Lmemset_byte

    # SSE2 路径
    movl %cr0, %ecx
    pushl %ecx               # 保存 CR0
    andl $0xFFFFFFF7, %ecx   # 清 CR0.TS (bit 3)
    movl %ecx, %cr0

    subl $16, %esp
    movdqu %xmm0, (%esp)     # 保存 xmm0 (栈不保证 16B 对齐)

    # 广播 value 到 xmm0 (16 字节)
    movb %al, %ah
    movl %eax, %ecx
    shll $16, %ecx
    orl %ecx, %eax           # eax = 4 字节广播
    movd %eax, %xmm0
    pshufd $0, %xmm0, %xmm0

    # 对齐 dst 到 16B 边界
    movl %esi, %ecx
    negl %ecx
    andl $15, %ecx
    jz .Lmemset_sse2
    subl %ecx, %edx
.Lmemset_align:
    movb %al, (%esi)
    incl %esi
    decl %ecx
    jnz .Lmemset_align

.Lmemset_sse2:
    movl %edx, %ecx
    shrl $5, %ecx
    testl %ecx, %ecx
    jz .Lmemset_sse2_done
.Lmemset_sse2_loop:
    movdqa %xmm0, (%esi)
    movdqa %xmm0, 16(%esi)
    addl $32, %esi
    decl %ecx
    jnz .Lmemset_sse2_loop
.Lmemset_sse2_done:
    andl $31, %edx

    # 恢复 xmm0 和 CR0
    movdqu (%esp), %xmm0
    addl $16, %esp
    popl %ecx
    movl %ecx, %cr0

    testl %edx, %edx
    jz .Lmemset_ret

.Lmemset_byte:
    movzbl 12(%ebp), %eax
.Lmemset_byte_loop:
    movb %al, (%esi)
    incl %esi
    decl %edx
    jnz .Lmemset_byte_loop

.Lmemset_ret:
    popl %esi
    popl %ebp
    ret
.section .note.GNU-stack,"",%progbits
