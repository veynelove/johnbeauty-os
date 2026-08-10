.set MAGIC, 0x1badb002
.set FLAGS, (1 << 0 | 1 << 1)
.set CHECKSUM, -(MAGIC + FLAGS)

/* Multiboot 头 (ALLOC: GRUB 在首 8KB 内找 magic) */
.section .multiboot, "a"
.align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

/* .boot 段: VMA == LMA == 物理地址, GRUB 未开分页时直接执行 */
.section .boot, "awx"
.align 4096
boot_page_dir:
    .fill 1024, 4, 0

.align 16
boot_stack:
    .space 16384
boot_stack_top:

.global loader
.type loader, @function
loader:
    movl  $boot_stack_top, %esp

    cmpl  $0x2badb002, %eax
    jne   _stop_bad_magic

    /* 保存 mbinfo 物理地址和 magic */
    pushl %ebx
    pushl %eax

    /* 启动页表: 4MB 大页, 恒等 + 高半核各映射 16MB */
    movl  $0x83 + 0x00000000, boot_page_dir + ((0 + 0) * 4)
    movl  $0x83 + 0x00400000, boot_page_dir + ((0 + 1) * 4)
    movl  $0x83 + 0x00800000, boot_page_dir + ((0 + 2) * 4)
    movl  $0x83 + 0x00C00000, boot_page_dir + ((0 + 3) * 4)

    movl  $0x83 + 0x00000000, boot_page_dir + ((768 + 0) * 4)
    movl  $0x83 + 0x00400000, boot_page_dir + ((768 + 1) * 4)
    movl  $0x83 + 0x00800000, boot_page_dir + ((768 + 2) * 4)
    movl  $0x83 + 0x00C00000, boot_page_dir + ((768 + 3) * 4)

    /* 开分页: CR4.PSE → CR3 → CR0.PG → jmp 序列化 */
    movl  %cr4, %eax
    orl   $0x10, %eax
    movl  %eax, %cr4

    movl  $boot_page_dir, %eax
    movl  %eax, %cr3

    movl  %cr0, %eax
    orl   $0x80000000, %eax
    movl  %eax, %cr0
    jmp   .Lpg_flush
.Lpg_flush:

    lea   high_half, %ecx
    jmp   *%ecx

_stop_bad_magic:
1:  cli
    hlt
    jmp   1b

.size loader, . - loader

/* high_half: 链接在 .text, VMA = 0xC01xxxxx */
.section .text
.extern john_beauty_main
.extern call_constructors
.extern _kernel_end
.extern _bss_start
.extern _bss_end

.type high_half, @function
high_half:
    /* 切永久内核栈 */
    movl  $kernel_stack, %esp

    /* 清零 BSS */
    movl  $_bss_start, %edi
    movl  $_bss_end, %ecx
    subl  %edi, %ecx
    xorl  %eax, %eax
    cld
    rep   stosb

    call  call_constructors

    /* mbinfo 物理地址 → 虚拟地址 */
    popl  %eax
    popl  %ebx
    addl  $0xC0000000, %ebx

    pushl $_kernel_end
    pushl %ebx
    call  john_beauty_main

1:
    cli
    hlt
    jmp   1b
.size high_half, . - high_half

/* 永久内核栈 (BSS) */
.section .bss, "aw", @nobits
.align 16
    .space 4 * 1024 * 1024
kernel_stack:
