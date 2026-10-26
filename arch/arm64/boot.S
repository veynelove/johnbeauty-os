/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

/* GAS has no C23-style digit separators: plain hex only in asm */
.set JLOS_IMAGE_CODE0_MZ_ADD,   0x91005a4d
.set JLOS_IMAGE_TEXT_OFFSET,    0x00080000
.set JLOS_IMAGE_FLAGS,          0
.set JLOS_IMAGE_MAGIC,          0x644d5241
.set JLOS_PE_LFANEW_OFFSET,     0x40
.set JLOS_AARCH64_EL1,          0x4
.set JLOS_AARCH64_EL2,          0x8
.set JLOS_AARCH64_DAIF_ALL,     0xF
.set JLOS_AARCH64_SPSEL_EL1,    0x1
.set JLOS_AARCH64_SPSR_EL1H,    0x3C5

.set JLOS_AARCH64_SPSR_EL2H,    0x3C9
.set JLOS_AARCH64_SCR_EL3_RW_NS, ((1 << 10) | (1 << 0))

.set JLOS_ARM64_PAGE_SIZE,      4096
.set JLOS_ARM64_L0_T0_OFF,      (0 * JLOS_ARM64_PAGE_SIZE)
.set JLOS_ARM64_L0_T1_OFF,      (1 * JLOS_ARM64_PAGE_SIZE)
.set JLOS_ARM64_L1_DMAP_OFF,    (2 * JLOS_ARM64_PAGE_SIZE)
.set JLOS_ARM64_L1_KIMAGE_OFF,  (3 * JLOS_ARM64_PAGE_SIZE)
.set JLOS_ARM64_L1_IDMAP_OFF,   (4 * JLOS_ARM64_PAGE_SIZE)
.set JLOS_ARM64_L0_IDX_DMAP,    0
.set JLOS_ARM64_L0_IDX_KIMAGE,  0x100
.set JLOS_ARM64_L0_IDX_IDMAP,   0
.set JLOS_ARM64_L1_IDX_MMIO,    0
.set JLOS_ARM64_L1_IDX_RAM,     1
.set JLOS_ARM64_DESC_TABLE,     0x3
.set JLOS_ARM64_DESC_BLOCK_DEV, 0x401
.set JLOS_ARM64_DESC_BLOCK_NRM, 0x705
.set JLOS_ARM64_PA_OFFSET,      0x0000800000000000
.set JLOS_ARM64_RAM_BASE,       0x40000000


.section .text.boot, "ax"
.global _start
.type _start, %function
_start:
    .long   JLOS_IMAGE_CODE0_MZ_ADD
    b       stext
    .quad   JLOS_IMAGE_TEXT_OFFSET
    /* GAS cannot emit a relocation for undefined - undefined; use the
     * in-file _start label (same address as __image_start) so it folds
     * into the addend. Linux head.S uses the same pattern. */
    .quad   __image_end - _start             /* 0x10: image_size */
    .quad   JLOS_IMAGE_FLAGS
    .quad   0
    .quad   0
    .quad   0
    .long   JLOS_IMAGE_MAGIC
    .long   JLOS_PE_LFANEW_OFFSET

    .balign 8
stext:
    mov     x28, x0                         /* preserve DTB pointer */

    mrs     x9, CurrentEL
    cmp     x9, #0xC
    b.eq    el3_drop_to_el2
    cmp     x9, #JLOS_AARCH64_EL2
    b.eq    el2_drop_to_el1
    cmp     x9, #JLOS_AARCH64_EL1
    b.ne    boot_halt

el1_entry:
    msr     daifset, #JLOS_AARCH64_DAIF_ALL
    msr     spsel, #JLOS_AARCH64_SPSEL_EL1

    bl      create_page_tables              /* returns x5 = page table PA */

    ldr     x0, =0xFF04                     /* MAIR: attr0 device, attr1 normal wb */
    msr     mair_el1, x0

    ldr     x0, =(0x10 | (0x3<<8) | (0x3<<10) | (0x3<<12) | \
                (0x10<<16) | (0x3<<24) | (0x3<<26) | (0x3<<28))
    msr     tcr_el1, x0

    mov     x1, x5                          /* TTBR0: empty user L0 */
    ldr     x2, =JLOS_ARM64_L0_T1_OFF
    add     x2, x2, x5                      /* TTBR1: kernel L0 */
    msr     ttbr0_el1, x1
    msr     ttbr1_el1, x2

    dsb     nsh
    ic      iallu
    dsb     nsh
    isb

    mrs     x0, sctlr_el1
    orr     x0, x0, #1
    orr     x0, x0, #4
    orr     x0, x0, #0x1000
    bic     x0, x0, #2
    msr     sctlr_el1, x0
    isb

    ldr     x9, =__primary_switched
    br      x9

create_page_tables:
    ldr     x0, =__pgtable_start
    movz    x1, #0x8000, lsl #32
    add     x5, x0, x1                      /* x5 = page table PA base */

    ldr     x1, =(5 * JLOS_ARM64_PAGE_SIZE)
    add     x1, x1, x5
    mov     x0, x5
.L_pg_zero:
    stp     xzr, xzr, [x0], #16
    cmp     x0, x1
    b.lo    .L_pg_zero

    ldr     x2, =JLOS_ARM64_L0_T1_OFF
    ldr     x3, =JLOS_ARM64_L1_DMAP_OFF
    ldr     x4, =JLOS_ARM64_L1_KIMAGE_OFF
    ldr     x6, =JLOS_ARM64_L1_IDMAP_OFF
    add     x2, x2, x5
    add     x3, x3, x5
    add     x4, x4, x5
    add     x6, x6, x5

    add     x0, x3, #JLOS_ARM64_DESC_TABLE
    str     x0, [x2, #(JLOS_ARM64_L0_IDX_DMAP << 3)]
    add     x0, x4, #JLOS_ARM64_DESC_TABLE
    str     x0, [x2, #(JLOS_ARM64_L0_IDX_KIMAGE << 3)]
    add     x0, x6, #JLOS_ARM64_DESC_TABLE
    str     x0, [x5, #(JLOS_ARM64_L0_IDX_IDMAP << 3)]

    ldr     x0, =JLOS_ARM64_DESC_BLOCK_DEV
    str     x0, [x3, #(JLOS_ARM64_L1_IDX_MMIO << 3)]
    ldr     x0, =(JLOS_ARM64_RAM_BASE | JLOS_ARM64_DESC_BLOCK_NRM)
    str     x0, [x3, #(JLOS_ARM64_L1_IDX_RAM << 3)]
    str     x0, [x4, #(JLOS_ARM64_L1_IDX_RAM << 3)]
    str     x0, [x6, #(JLOS_ARM64_L1_IDX_RAM << 3)]

    ret

__primary_switched:
    msr     daifset, #JLOS_AARCH64_DAIF_ALL

    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb

    ldr     x9, =__boot_stack_top
    mov     sp, x9

    ldr     x10, =__bss_start
    ldr     x11, =__bss_end
    mov     x12, #0
.L_bss_zero:
    cmp     x10, x11
    b.hs    .L_bss_done
    str     x12, [x10], #8
    b       .L_bss_zero
.L_bss_done:
    mov     x0, x28
    bl      start_main

boot_halt:
.L_halt:
    wfi
    b       .L_halt

el3_drop_to_el2:
    msr     daifset, #JLOS_AARCH64_DAIF_ALL
    /* 真硬件裸启：RW=1 下层 AArch64，NS=1 进入非安全态（GIC 非安全视图） */
    ldr     x9, =JLOS_AARCH64_SCR_EL3_RW_NS
    msr     scr_el3, x9
    isb
    ldr     x9, =JLOS_AARCH64_SPSR_EL2H
    msr     spsr_el3, x9
    adr     x9, el2_drop_to_el1
    msr     elr_el3, x9
    eret

el2_drop_to_el1:
    msr     daifset, #JLOS_AARCH64_DAIF_ALL
    /* EL1PCEN|EL1PCTEN: 允许 EL1 访问物理定时器/计数器（真硬件复位为 0 会 trap） */
    mrs     x9, cnthctl_el2
    orr     x9, x9, #0x3
    msr     cnthctl_el2, x9
    /* SRE|Enable: 打开 GICv3 CPU 接口系统寄存器访问（GIC-600 复位 SRE=0） */
    mrs     x9, icc_sre_el2
    orr     x9, x9, #0x1
    orr     x9, x9, #0x8
    msr     icc_sre_el2, x9
    isb
    ldr     x9, =JLOS_AARCH64_SPSR_EL1H
    msr     spsr_el2, x9
    adr     x9, el1_entry
    msr     elr_el2, x9
    eret

.global _secondary_start
.type _secondary_start, %function
_secondary_start:
    msr     daifset, #JLOS_AARCH64_DAIF_ALL
    mov     x19, x0                         /* context: AP stack top VA */


    mrs     x9, CurrentEL
    cmp     x9, #JLOS_AARCH64_EL2
    b.eq    sec_drop_to_el1
    cmp     x9, #JLOS_AARCH64_EL1
    b.ne    boot_halt

sec_setup:
    msr     spsel, #JLOS_AARCH64_SPSEL_EL1

    ldr     x0, =__pgtable_start
    movz    x1, #0x8000, lsl #32
    add     x0, x0, x1                      /* x0 = page table PA base */

    ldr     x1, =JLOS_ARM64_L1_IDMAP_OFF
    add     x1, x1, x0                      /* x1 = L1_IDMAP table PA */
    ldr     x2, =JLOS_ARM64_RAM_BASE
    add     x2, x2, #JLOS_ARM64_DESC_BLOCK_NRM
    str     x2, [x1, #(JLOS_ARM64_L1_IDX_RAM << 3)]
    add     x2, x1, #JLOS_ARM64_DESC_TABLE
    str     x2, [x0, #(JLOS_ARM64_L0_IDX_IDMAP << 3)]

    msr     ttbr0_el1, x0
    ldr     x1, =JLOS_ARM64_L0_T1_OFF
    add     x1, x1, x0
    msr     ttbr1_el1, x1

    ldr     x0, =0xFF04
    msr     mair_el1, x0
    ldr     x0, =(0x10 | (0x3<<8) | (0x3<<10) | (0x3<<12) | \
                (0x10<<16) | (0x3<<24) | (0x3<<26) | (0x3<<28))
    msr     tcr_el1, x0

    dsb     nsh
    ic      iallu
    dsb     nsh
    isb

    mrs     x0, sctlr_el1
    orr     x0, x0, #1
    orr     x0, x0, #4
    orr     x0, x0, #0x1000
    bic     x0, x0, #2
    msr     sctlr_el1, x0
    isb

    mov     sp, x19
    ldr     x9, =__secondary_switched
    br      x9

sec_drop_to_el1:
    mrs     x9, cnthctl_el2
    orr     x9, x9, #0x3
    msr     cnthctl_el2, x9
    mrs     x9, icc_sre_el2
    orr     x9, x9, #0x1
    orr     x9, x9, #0x8
    msr     icc_sre_el2, x9
    isb
    ldr     x9, =JLOS_AARCH64_SPSR_EL1H
    msr     spsr_el2, x9
    adr     x9, sec_setup
    msr     elr_el2, x9
    eret

__secondary_switched:

    ldr     x0, =vectors
    msr     vbar_el1, x0
    isb
    bl      jlos_ap_main
    b       boot_halt

.section .note.GNU-stack,"",%progbits
