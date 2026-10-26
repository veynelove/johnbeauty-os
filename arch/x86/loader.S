/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

.include "arch/x86/multiboot.inc"

.set MAGIC, MULTIBOOT_HEADER_MAGIC

#ifdef CONFIG_X86_64
.set FLAGS, MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE | MULTIBOOT_AOUT_KLUDGE
#else
.set FLAGS, MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE
#endif

.set CHECKSUM, -(MAGIC + FLAGS)

.set KERNEL_CS64, 0x08
.set KERNEL_DS64, 0x10

/* Multiboot 头（64 位下用 AOUT_KLUDGE 让 GRUB 按二进制加载，绕过 ELF64 解析） */
.section .multiboot, "a"
.align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
#ifdef CONFIG_X86_64
    .long _boot_start
    .long _boot_start
    .long _bss_start_phys
    .long _kernel_end_phys
    .long loader
#else
    .long 0             /* header_addr (AOUT_KLUDGE 未设置, 占位) */
    .long 0             /* load_addr */
    .long 0             /* load_end_addr */
    .long 0             /* bss_end_addr */
    .long 0             /* entry_addr */
#endif
    .long 0             /* mode_type: 0 = linear framebuffer */
    .long 1024          /* width */
    .long 768           /* height */
    .long 32            /* depth (bpp) */

/* .boot 段: VMA == LMA == 物理地址, GRUB 未开分页时直接执行 */
.section .boot, "awx"

#ifdef CONFIG_X86_64

/* ============ 64 位：4 级页表（静态定义，符号值=物理地址） ============ */
/* 恒等映射 [0, 64MB) + 高半 (-2GB, +64MB) + direct map 低 64MB，共用同一个 PD，2MB 大页 */
/* PML4[256] direct map: initialize_kernel 在 boot 页表下运行时, PHYS_TO_VIRT (direct map 区)
 * 返回的地址必须可访问, 否则 PD/PT 分配后 memset 即崩溃; kernel PML4 enable 后由运行时
 * direct map (覆盖全部物理内存) 替代本静态映射 */

.align 4096
.global boot_pml4
boot_pml4:
    .quad boot_pdpt_low + 0x3
    .fill 255, 8, 0
    .quad boot_pdpt_direct + 0x3
    .fill 254, 8, 0
    .quad boot_pdpt_high + 0x3

.align 4096
boot_pdpt_low:
    .quad boot_pd + 0x3
    .fill 511, 8, 0

.align 4096
boot_pdpt_direct:
    .quad boot_pd + 0x3
    .fill 511, 8, 0

.align 4096
boot_pdpt_high:
    .fill 510, 8, 0
    .quad boot_pd + 0x3
    .fill 1, 8, 0

.align 4096
boot_pd:
    .set PD_IDX, 0
    .rept 32
    .quad (PD_IDX * 0x200000) + 0x83    /* 物理 [0,64MB) 2MB 大页: present|rw|ps */
    .set PD_IDX, PD_IDX + 1
    .endr
    .fill 480, 8, 0

/* 64 位 GDT（lgdt 在 32 位模式下执行，读 6 字节 GDTR） */
.align 16
boot_gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF    /* 64 位代码段 ring0, sel=0x08, L=1 */
    .quad 0x00CF92000000FFFF    /* 数据段 ring0, sel=0x10 */
boot_gdt64_end:
boot_gdt64_desc:
    .word boot_gdt64_end - boot_gdt64 - 1
    .long boot_gdt64
#else
/* ============ 32 位：boot_page_dir（运行时填充） ============ */
.align 4096
.global boot_page_dir
boot_page_dir:
    .fill 1024, 4, 0
#endif

.align 16
boot_stack:
    .space 16384
boot_stack_top:

/* 在 .boot 段保存 multiboot 参数 (BSS 清零不影响) */
.global boot_mbinfo_pa
boot_mbinfo_pa:
    .long 0
.global boot_magic
boot_magic:
    .long 0

.global loader
.type loader, @function
.code32
loader:
    movl  $boot_stack_top, %esp

    cmpl  $MULTIBOOT_BOOTLOADER_MAGIC, %eax
    jne   _stop_bad_magic

    /* 保存 mbinfo 物理地址和 magic 到 .boot 段全局变量 */
    movl  %ebx, boot_mbinfo_pa
    movl  %eax, boot_magic

#ifdef CONFIG_X86_64
    movl  $0x80000000, %eax
    cpuid
    cmpl  $0x80000001, %eax
    jb    .Lon_long_mode
    movl  $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz    .Lon_long_mode

    /* ---- 开 long mode: CR4.PAE -> IA32_EFER.LME -> CR3 -> CR0.PG ---- */
    /* CR4: PAE(bit5,4级页表) | OSFXSR(bit9,启用FXSAVE/FXRSTOR/SSE) */
    movl  %cr4, %eax
    orl   $0x220, %eax
    movl  %eax, %cr4

    movl  $0xC0000080, %ecx
    rdmsr
    orl   $(1 << 8), %eax
    wrmsr

    movl  $boot_pml4, %eax
    movl  %eax, %cr3

    movl  %cr0, %eax
    orl   $(1 << 31), %eax
    movl  %eax, %cr0

    /* ---- 加载 64 位 GDT + 远跳到 64 位入口（物理地址，恒等映射） ---- */
    movl  $boot_gdt64_desc, %eax
    lgdt  (%eax)

    ljmp  $KERNEL_CS64, $kernel_entry64

.Lon_long_mode:
1:
    cli
    hlt
    jmp  1b
#else
    /* 启动页表: 4MB 大页, 恒等 + 高半核各映射 64MB */
    movl  $0x00, %ecx
    movl  $0x83, %eax
1:
    movl  %eax, boot_page_dir(, %ecx, 4)
    movl  %eax, boot_page_dir + (768 * 4)(, %ecx, 4)
    addl  $0x00400000, %eax
    incl  %ecx
    cmpl  $16, %ecx
    jl    1b

    /* 开分页: CR4.PSE → CR3 → CR0.PG → jmp 序列化 */
    /* CR4: PSE(bit4,4MB页) | OSFXSR(bit9,启用FXSAVE/FXRSTOR/SSE) */
    movl  %cr4, %eax
    orl   $0x210, %eax
    movl  %eax, %cr4

    movl  $boot_page_dir, %eax
    movl  %eax, %cr3

    movl  %cr0, %eax
    andl  $~0x4, %eax
    orl   $0x80000022, %eax /* PG(bit 31) | MP(bit 1) | NE(bit 5) */ 
    movl  %eax, %cr0
    jmp   .Lpg_flush
.Lpg_flush:

    lea   high_half, %ecx
    jmp   *%ecx
#endif

_stop_bad_magic:
1:
    cli
    hlt
    jmp   1b

.size loader, . - loader

#ifdef CONFIG_X86_64
/* ---- 64 位入口（仍在 .boot 段，物理地址，恒等映射执行） ---- */
.code64
.extern john_sunshine_main
.extern _kernel_end
.global kernel_entry64
kernel_entry64:
    /* 加载数据段 */
    movw  $KERNEL_DS64, %ax
    movw  %ax, %ds
    movw  %ax, %es
    movw  %ax, %fs
    movw  %ax, %gs
    movw  %ax, %ss

    /* 切永久内核栈（高半核 VA，.bss 中，PML4[511] 映射） */
    movq  $kernel_stack, %rsp

    /* mbinfo PA → direct map VA（PML4[256]，boot + kernel 页表均映射） */
    movl  boot_mbinfo_pa, %eax
    movq  $0xFFFF800000000000, %rcx
    addq  %rcx, %rax
    movq  %rax, %rdi                /* arg1 = mbinfo VA */

    movq  $jlos_x86_boot_info_from, %rax
    call  *%rax                     /* rax = jlos_hal_boot_info_t * */

    movq  %rax, %rdi                /* arg1 = boot_info */
    movq  $_kernel_end, %rsi        /* arg2 = kernel_end VA */

    /* 绝对间接调用（PC-relative 无法从 .boot 物理地址直达高半核） */
    movq  $john_sunshine_main, %rax
    call  *%rax

1:
    cli
    hlt
    jmp  1b

.section .bss, "aw", @nobits
.align 16
.global kernel_stack_bottom
kernel_stack_bottom:
    .space 4 * 1024 * 1024
.global kernel_stack
kernel_stack:
#else
/* ---- 32 位 high_half（链接在 .text 高半核） ---- */
.section .text
.extern john_sunshine_main
.extern _kernel_end
.extern _bss_start
.extern _bss_end

.type high_half, @function
.code32
high_half:
    /* 从 .boot 段读取 multiboot 参数 (高半核映射: boot_mbinfo_pa PA → VA = PA + KERNEL_VIRTUAL_BASE) */
    movl  boot_mbinfo_pa, %ebx
    movl  boot_magic, %eax

    /* 切永久内核栈 */
    movl  $kernel_stack, %esp

    /* 清零 BSS (不影响 .boot 段的 boot_mbinfo_pa/boot_magic) */
    movl  $_bss_start, %edi
    movl  $_bss_end, %ecx
    subl  %edi, %ecx
    xorl  %eax, %eax
    cld
    rep   stosb

    /* mbinfo 物理地址 → 虚拟地址 */
    movl  boot_mbinfo_pa, %ebx
    addl  $0xC0000000, %ebx

    pushl %ebx
    call  jlos_x86_boot_info_from    /* eax = jlos_hal_boot_info_t * */
    addl  $4, %esp

    pushl $_kernel_end
    pushl %eax
    call  john_sunshine_main

1:
    cli
    hlt
    jmp   1b
.size high_half, . - high_half

/* 永久内核栈 (BSS) */
.section .bss, "aw", @nobits
.align 16
.global kernel_stack_bottom
kernel_stack_bottom:
    .space 4 * 1024 * 1024
.global kernel_stack
kernel_stack:
#endif

.section .note.GNU-stack,"",%progbits
