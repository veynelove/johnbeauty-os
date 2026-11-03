/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GDT_H
#define _GDT_H

#include <common/types.h>

#define JLOS_X86_KERNEL_CS_SEL          0x10
#define JLOS_X86_KERNEL_DS_SEL          0x18
#define JLOS_X86_USER_CS                0x23
#define JLOS_X86_USER_DS                0x2B
#ifdef CONFIG_X86_64
#define JLOS_X86_PERCPU_SEL             0x40
#else
#define JLOS_X86_PERCPU_SEL             0x38
#endif

#define JLOS_GDT_ACCESS_KERNEL_CODE     0x9A
#define JLOS_GDT_ACCESS_KERNEL_DATA     0x92
#define JLOS_GDT_ACCESS_USER_CODE       0xFA
#define JLOS_GDT_ACCESS_USER_DATA       0xF2
#define JLOS_GDT_ACCESS_TSS             0x89
#define JLOS_GDT_ACCESS_SYSTEM_BIT      0x10
#define JLOS_GDT_ACCESS_EXEC_BIT        0x08
#define JLOS_GDT_ACCESS_CODE_SEG        (JLOS_GDT_ACCESS_SYSTEM_BIT | JLOS_GDT_ACCESS_EXEC_BIT)

#define JLOS_GDT_FLAGS_16BIT            0x40
#define JLOS_GDT_FLAGS_32BIT            0xC0
#define JLOS_GDT_FLAGS_64BIT            0xA0
#define JLOS_GDT_FLAGS_LIMIT_HI_MASK    0x0F
#define JLOS_GDT_FLAGS_GRAN_MASK        0xC0

#define JLOS_X86_ASM_STR(x)     #x
#define JLOS_X86_ASM_XSTR(x)    JLOS_X86_ASM_STR(x)

typedef struct jlos_gdt_segment_descriptor {
    uint16_t    limit_lo;
    uint16_t    base_lo;
    uint8_t     base_hi;
    uint8_t     type;
    uint8_t     flags_limit_hi;
    uint8_t     base_vhi;
} __attribute__((packed)) jlos_gdt_segment_descriptor_t;

#ifdef CONFIG_X86_64
typedef struct {
    uint16_t    limit_lo;
    uint16_t    base_lo;
    uint8_t     base_hi;
    uint8_t     type;
    uint8_t     flags_limit_hi;
    uint8_t     base_vhi;
    uint32_t    base_xhi;
    uint32_t    reserved;
} __attribute__((packed)) jlos_x86_tss_descriptor_t;
#endif

typedef struct jlos_mmu {
    jlos_gdt_segment_descriptor_t null_segment_selector;
    jlos_gdt_segment_descriptor_t unused_segment_selector;
    jlos_gdt_segment_descriptor_t code_segment_selector;
    jlos_gdt_segment_descriptor_t data_segment_selector;
    jlos_gdt_segment_descriptor_t user_code_segment_selector;
    jlos_gdt_segment_descriptor_t user_data_segment_selector;
#ifdef CONFIG_X86_64
    jlos_x86_tss_descriptor_t tss_segment_selector;
#else
    jlos_gdt_segment_descriptor_t tss_segment_selector;
#endif
    jlos_gdt_segment_descriptor_t per_cpu_segment_selector;
} __attribute__((packed)) jlos_mmu_t;

void jlos_mmu_init(void);
void jlos_arch_cpu_gdt_init(uint32_t cpu);

uint16_t jlos_mmu_code_selector(jlos_mmu_t* self);
uint16_t jlos_mmu_data_selector(jlos_mmu_t* self);
uint16_t jlos_mmu_user_code_selector(jlos_mmu_t *self);
uint16_t jlos_mmu_user_data_selector(jlos_mmu_t *self);
jlos_mmu_t *jlos_mmu_get_kernel(void);

uint16_t jlos_gdt_tss_selector(jlos_mmu_t *self);
void jlos_gdt_set_tss(jlos_mmu_t *self, uintptr_t base, uint32_t limit);

void jlos_gdt_segment_descriptor_init(jlos_gdt_segment_descriptor_t* self, uint32_t base, uint32_t limit, uint8_t flags);
uint32_t jlos_gdt_segment_descriptor_base(jlos_gdt_segment_descriptor_t* self);
uint32_t jlos_gdt_segment_descriptor_limit(jlos_gdt_segment_descriptor_t* self);

#endif