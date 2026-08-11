.include "arch/x86/multiboot.inc"

.set MAGIC, MULTIBOOT_HEADER_MAGIC
.set FLAGS, MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
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

/* 在 .boot 段保存 multiboot 参数 (BSS 清零不影响) */
.global boot_mbinfo_pa
boot_mbinfo_pa:
    .long 0
.global boot_magic
boot_magic:
    .long 0

.global loader
.type loader, @function
loader:
    movl  $boot_stack_top, %esp

    cmpl  $MULTIBOOT_BOOTLOADER_MAGIC, %eax
    jne   _stop_bad_magic

    /* 保存 mbinfo 物理地址和 magic 到 .boot 段全局变量 */
    movl  %ebx, boot_mbinfo_pa
    movl  %eax, boot_magic

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

    call  call_constructors

    /* mbinfo 物理地址 → 虚拟地址 */
    movl  boot_mbinfo_pa, %ebx
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
