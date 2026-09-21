# SSE2 优化 memcpy — 32B 宽读写
# void *jlos_memcpy(void *dst, const void *src, size_t size)
.text
.global jlos_memcpy
jlos_memcpy:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi

    movl 8(%ebp), %edi        # dst
    movl 12(%ebp), %esi       # src
    movl 16(%ebp), %edx       # size

    cmpl $64, %edx
    jb .Lmemcpy_byte

    # 检查 CR4.OSFXSR (bit 9) 是否已设置
    movl %cr4, %ecx
    testl $0x200, %ecx
    jz .Lmemcpy_byte

    # SSE2 路径
    movl %cr0, %ecx
    pushl %ecx               # 保存 CR0
    andl $0xFFFFFFF7, %ecx   # 清 CR0.TS
    movl %ecx, %cr0

    subl $16, %esp
    movdqu %xmm0, (%esp)     # 保存 xmm0 (栈不保证 16B 对齐)

    # 对齐 dst 到 16B 边界
    movl %edi, %ecx
    negl %ecx
    andl $15, %ecx
    jz .Lmemcpy_sse2
    cmpl %ecx, %edx
    jb .Lmemcpy_restore      # size < prefix
    subl %ecx, %edx
.Lmemcpy_align:
    movb (%esi), %al
    movb %al, (%edi)
    incl %esi
    incl %edi
    decl %ecx
    jnz .Lmemcpy_align

.Lmemcpy_sse2:
    movl %edx, %ecx
    shrl $5, %ecx
    testl %ecx, %ecx
    jz .Lmemcpy_sse2_done
.Lmemcpy_sse2_loop:
    movdqu (%esi), %xmm0
    movdqa %xmm0, (%edi)
    movdqu 16(%esi), %xmm0
    movdqa %xmm0, 16(%edi)
    addl $32, %esi
    addl $32, %edi
    decl %ecx
    jnz .Lmemcpy_sse2_loop
.Lmemcpy_sse2_done:
    andl $31, %edx

.Lmemcpy_restore:
    movdqu (%esp), %xmm0
    addl $16, %esp
    popl %ecx
    movl %ecx, %cr0

    testl %edx, %edx
    jz .Lmemcpy_ret

.Lmemcpy_byte:
.Lmemcpy_byte_loop:
    movb (%esi), %al
    movb %al, (%edi)
    incl %esi
    incl %edi
    decl %edx
    jnz .Lmemcpy_byte_loop

.Lmemcpy_ret:
    movl 8(%ebp), %eax       # 返回 dst
    popl %edi
    popl %esi
    popl %ebp
    ret
.section .note.GNU-stack,"",%progbits
