/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * AP 启动 trampoline: 运行时被复制到低物理 0x8000, AP 实模式从此处开始,
 * 切保护模式 → 开分页(内核 CR3) → 跳高半核 jlos_ap_main。
 *
 * 位置无关: 实模式用 16 位寻址, 运行时地址 = 0x8000 + (label - start)。
 * label 差在同 section 内为汇编期常量。所有绝对地址字段由 BSP 运行时填入。
 *
 * 注意: 0x8000 必须与 trampoline.h 的 JLOS_X86_AP_TRAMPOLINE_PHYS 保持一致。
 */

.section .trampoline, "ax"

#ifndef CONFIG_X86_64

/* ============ 32 位 trampoline ============ */

.code16
.global jlos_x86_trampoline_start
.global jlos_x86_trampoline_end
.global jlos_x86_trampoline_prot32
.global jlos_x86_trampoline_gdt_desc
.global jlos_x86_trampoline_gdt
.global jlos_x86_trampoline_far_ptr
.global jlos_x86_trampoline_high_ptr
.global jlos_x86_trampoline_cr4
.global jlos_x86_trampoline_cr3
.global jlos_x86_trampoline_stack

jlos_x86_trampoline_start:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs

    /* lgdt: GDT 伪描述符在 trampoline 内, 运行时地址 = 0x8000 + 偏移 */
    movw    $0x8000, %bx
    addw    $(jlos_x86_trampoline_gdt_desc - jlos_x86_trampoline_start), %bx
    lgdtl   (%bx)

    /* 开 PE: 清 NW/CD(bit29/30), 设 MP/NE(bit1/5), 对齐 BSP FPU 配置 */
    movl    %cr0, %eax
    andl    $0x9FFFFFFB, %eax
    orl     $0x00000023, %eax
    movl    %eax, %cr0

    /* 远跳 32 位保护模式: far pointer 在 far_ptr 字段 */
    movw    $0x8000, %bx
    addw    $(jlos_x86_trampoline_far_ptr - jlos_x86_trampoline_start), %bx
    ljmp    *(%bx)

.code32
jlos_x86_trampoline_prot32:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %ss

    /* 载入 CR4 (BSP 运行时填): PSE 等, 必须在开 PG 前设好,
     * 否则 4MB 页目录项被误解为页表指针 → #PF */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_cr4 - jlos_x86_trampoline_start), %ebx
    movl    (%ebx), %eax
    movl    %eax, %cr4

    /* 载入内核页目录 (BSP 运行时填) */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_cr3 - jlos_x86_trampoline_start), %ebx
    movl    (%ebx), %eax
    movl    %eax, %cr3

    /* 开 PG(bit31) + WP(bit16), 对齐 BSP CR0 */
    movl    %cr0, %eax
    orl     $0x80010000, %eax
    movl    %eax, %cr0

    /* AP 内核栈顶 (BSP 运行时填) */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_stack - jlos_x86_trampoline_start), %ebx
    movl    (%ebx), %esp

    /* 远跳高半核 jlos_ap_main: far pointer 在 high_ptr 字段 */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_high_ptr - jlos_x86_trampoline_start), %ebx
    ljmp    *(%ebx)

    .align 4
jlos_x86_trampoline_gdt_desc:
    .word   0x0017              /* limit: 3 项 * 8 - 1 */
    .long   0                   /* base: BSP 运行时填 (GDT 物理地址) */

jlos_x86_trampoline_gdt:
    .quad   0                   /* null */
    .quad   0x00CF9A000000FFFF  /* code: flat, ring0, 32-bit, exec/read */
    .quad   0x00CF92000000FFFF  /* data: flat, ring0, 32-bit, read/write */

    .align 4
jlos_x86_trampoline_far_ptr:
    .word   0                   /* offset: BSP 填 (prot32 运行时地址) */
    .word   0x08                /* selector: trampoline code */

    .align 4
jlos_x86_trampoline_high_ptr:
    .long   0                   /* offset: BSP 填 (jlos_ap_main 虚拟地址) */
    .word   0x08                /* selector: trampoline code */

    .align 4
jlos_x86_trampoline_cr4:
    .long   0                   /* BSP 填: CR4 (PSE|OSFXSR|PGE 等) */

    .align 4
jlos_x86_trampoline_cr3:
    .long   0                   /* BSP 填: 内核页目录物理地址 */

    .align 4
jlos_x86_trampoline_stack:
    .long   0                   /* BSP 填: AP 内核栈顶 */

jlos_x86_trampoline_end:
.section .note.GNU-stack,"",%progbits

#else

/* ============ 64 位 trampoline ============ */
/* 流程: 16 位实模式 → 32 位保护模式 → 设 EFER.LME → 开 PG → ljmp 64 位代码段
 * → prot64: 设栈 → jmp jlos_ap_main
 * GDT: null(0x00) / 32code(0x08) / 32data(0x10) / 64code(0x18) / 64data(0x20) */

.code16
.global jlos_x86_trampoline_start
.global jlos_x86_trampoline_end
.global jlos_x86_trampoline_prot32
.global jlos_x86_trampoline_prot64
.global jlos_x86_trampoline_gdt_desc
.global jlos_x86_trampoline_gdt
.global jlos_x86_trampoline_far_ptr
.global jlos_x86_trampoline_far64_ptr
.global jlos_x86_trampoline_high_ptr
.global jlos_x86_trampoline_cr4
.global jlos_x86_trampoline_cr3
.global jlos_x86_trampoline_stack

jlos_x86_trampoline_start:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs

    /* lgdt: GDT 伪描述符在 trampoline 内 */
    movw    $0x8000, %bx
    addw    $(jlos_x86_trampoline_gdt_desc - jlos_x86_trampoline_start), %bx
    lgdtl   (%bx)

    /* 开 PE: 清 NW/CD, 设 MP/NE, 对齐 BSP FPU 配置 */
    movl    %cr0, %eax
    andl    $0x9FFFFFFB, %eax
    orl     $0x00000023, %eax
    movl    %eax, %cr0

    /* 远跳 32 位保护模式 */
    movw    $0x8000, %bx
    addw    $(jlos_x86_trampoline_far_ptr - jlos_x86_trampoline_start), %bx
    ljmp    *(%bx)

.code32
jlos_x86_trampoline_prot32:
    movw    $0x10, %ax          /* 32 位 data selector */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %ss

    /* 载入 CR4 (BSP 运行时填): PAE 必须 1, 否则 4 级页表不工作 */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_cr4 - jlos_x86_trampoline_start), %ebx
    movl    (%ebx), %eax
    movl    %eax, %cr4

    /* 载入内核 PML4 物理地址 (BSP 运行时填) */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_cr3 - jlos_x86_trampoline_start), %ebx
    movl    (%ebx), %eax
    movl    %eax, %cr3

    /* 设 EFER.LME: 必须在 CR0.PG 前, 否则 CPU 进 32 位 PAE 而非 IA-32e → triple fault */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x00000100, %eax
    wrmsr

    /* 开 PG(bit31) + WP(bit16): EFER.LME=1 + PG: 0→1 → 进入 IA-32e 长模式 */
    movl    %cr0, %eax
    orl     $0x80010000, %eax
    movl    %eax, %cr0

    /* ljmp 64 位代码段 → prot64: 触发 64 位模式执行 */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_far64_ptr - jlos_x86_trampoline_start), %ebx
    ljmp    *(%ebx)

.code64
jlos_x86_trampoline_prot64:
    movw    $0x20, %ax          /* 64 位 data selector */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %ss

    /* AP 内核栈顶 (BSP 运行时填, 8 字节) */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_stack - jlos_x86_trampoline_start), %ebx
    movq    (%rbx), %rsp

    /* 跳 jlos_ap_main (BSP 运行时填 8 字节地址) */
    movl    $0x8000, %ebx
    addl    $(jlos_x86_trampoline_high_ptr - jlos_x86_trampoline_start), %ebx
    movq    (%rbx), %rax
    jmpq    *%rax

    .align 4
jlos_x86_trampoline_gdt_desc:
    .word   0x0027              /* limit: 5 项 * 8 - 1 */
    .long   0                   /* base: BSP 运行时填 (GDT 物理地址) */

jlos_x86_trampoline_gdt:
    .quad   0                   /* null */
    .quad   0x00CF9A000000FFFF  /* 32 位 code: flat, ring0, 32-bit, exec/read */
    .quad   0x00CF92000000FFFF  /* 32 位 data: flat, ring0, 32-bit, read/write */
    .quad   0x00AF9A000000FFFF  /* 64 位 code: L=1, D=0, ring0, exec/read */
    .quad   0x00CF92000000FFFF  /* 64 位 data: L=0, D=1, ring0, read/write */

    .align 4
jlos_x86_trampoline_far_ptr:
    .word   0                   /* offset: BSP 填 (prot32 运行时地址) */
    .word   0x08                /* selector: 32 位 code */

    .align 4
jlos_x86_trampoline_far64_ptr:
    .long   0                   /* offset: BSP 填 (prot64 运行时地址) */
    .word   0x18                /* selector: 64 位 code */

    .align 8
jlos_x86_trampoline_high_ptr:
    .quad   0                   /* BSP 填: jlos_ap_main 虚拟地址 (8 字节) */

    .align 4
jlos_x86_trampoline_cr4:
    .long   0                   /* BSP 填: CR4 (PAE|OSFXSR|PGE 等) */

    .align 8
jlos_x86_trampoline_cr3:
    .quad   0                   /* BSP 填: 内核 PML4 物理地址 */

    .align 8
jlos_x86_trampoline_stack:
    .quad   0                   /* BSP 填: AP 内核栈顶 (8 字节) */

jlos_x86_trampoline_end:
.section .note.GNU-stack,"",%progbits

#endif
