/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

/* plain hex: cpp passes C23 digit separators through and ld rejects them */
#define JLOS_ARM64_IMAGE_BASE       0xFFFF800040080000
#define JLOS_ARM64_BOOT_STACK_SIZE  (16 * 1024)
#define JLOS_ARM64_MAX_CPUS         8
#define JLOS_ARM64_PGTABLE_SIZE     (5 * 4096)

OUTPUT_ARCH(aarch64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R+X */
    data PT_LOAD FLAGS(6);   /* R+W */
}

SECTIONS
{
    . = JLOS_ARM64_IMAGE_BASE;

    __image_start = .;

    .text : {
        KEEP(*(.text.boot))
        . = ALIGN(2048);
        KEEP(*(.vectors))
        *(.text .text.*)
    } :text

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
    } :text

    .data..percpu : ALIGN(64) {
        __per_cpu_start = .;
        *(.data..percpu)
        __per_cpu_end = .;
    } :data

    .data : ALIGN(8) {
        *(.data .data.*)
        . = ALIGN(16);
        __initcall0_start = .; KEEP(*(.initcall0))  __initcall0_end = .;
        . = ALIGN(16);
        __initcall1_start = .; KEEP(*(.initcall1))  __initcall1_end = .;
        . = ALIGN(16);
        __initcall2_start = .; KEEP(*(.initcall2))  __initcall2_end = .;
        . = ALIGN(16);
        __initcall3_start = .; KEEP(*(.initcall3))  __initcall3_end = .;
        . = ALIGN(16);
        __initcall4_start = .; KEEP(*(.initcall4))  __initcall4_end = .;
        . = ALIGN(16);
        __initcall5_start = .; KEEP(*(.initcall5))  __initcall5_end = .;
        . = ALIGN(16);
        __jlos_test_start = .; KEEP(*(.jlos_test))  __jlos_test_end = .;
    } :data


    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } :data

    .init.pgtable (NOLOAD) : ALIGN(4096) {
        __pgtable_start = .;
        . += JLOS_ARM64_PGTABLE_SIZE;
        __pgtable_end = .;
    } :data

    .bootstack (NOLOAD) : ALIGN(16) {
        __boot_stack_base = .;
        . += JLOS_ARM64_BOOT_STACK_SIZE;
        __boot_stack_top = .;
    } :data

    .apstack (NOLOAD) : ALIGN(16) {
        __ap_boot_stack_base = .;
        . += JLOS_ARM64_BOOT_STACK_SIZE * JLOS_ARM64_MAX_CPUS;
        __ap_idle_stack_base = .;
        . += JLOS_ARM64_BOOT_STACK_SIZE * JLOS_ARM64_MAX_CPUS;
        __ap_stack_end = .;
    } :data

    . = ALIGN(0x1000);
    __image_end = .;

    _boot_end_phys = __image_end;
    _kernel_end_phys = __image_end;
}
