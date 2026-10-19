/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/gdt.h>
#include <arch/x86/smp.h>
#include <hal/mmu.h>
#include <hal/paging.h>
#include <hal/smp.h>

extern uint8_t _text_start,    _text_end;
extern uint8_t _rodata_start,  _rodata_end;
extern uint8_t _data_start,    _data_end;
extern uint8_t _bss_start,     _bss_end;

static jlos_hal_kernel_segments_t s_kernel_segments = {0};

void jlos_hal_kernel_segments_init(void)
{
    s_kernel_segments.text_start    = (uintptr_t)&_text_start;
    s_kernel_segments.text_end      = (uintptr_t)&_text_end;
    s_kernel_segments.rodata_start  = (uintptr_t)&_rodata_start;
    s_kernel_segments.rodata_end    = (uintptr_t)&_rodata_end;
    s_kernel_segments.data_start    = (uintptr_t)&_data_start;
    s_kernel_segments.data_end      = (uintptr_t)&_data_end;
    s_kernel_segments.bss_start     = (uintptr_t)&_bss_start;
    s_kernel_segments.bss_end       = (uintptr_t)&_bss_end;
}

const jlos_hal_kernel_segments_t *jlos_hal_get_kernel_segments(void)
{
    return &s_kernel_segments;
}

void jlos_gdt_segment_descriptor_init(jlos_gdt_segment_descriptor_t* self, uint32_t base, uint32_t limit, uint8_t flags)
{
    uint8_t flags_limit_hi = 0;
    if (limit <= 65536) {
        flags_limit_hi = JLOS_GDT_FLAGS_16BIT;
    }
    else {
        if ((limit & 0xFFF) != 0xFFF) {
            limit = (limit >> 12) - 1;
        }
        else {
            limit = limit >> 12;
        }
        flags_limit_hi = JLOS_GDT_FLAGS_32BIT;
    }
#ifdef CONFIG_X86_64
    if ((flags & JLOS_GDT_ACCESS_CODE_SEG) == JLOS_GDT_ACCESS_CODE_SEG) {
        flags_limit_hi = JLOS_GDT_FLAGS_64BIT;
    }
    else if (!(flags & JLOS_GDT_ACCESS_SYSTEM_BIT)) {
        flags_limit_hi = 0;
    }
#endif
    flags_limit_hi |= (uint8_t)((limit >> 16) & JLOS_GDT_FLAGS_LIMIT_HI_MASK);

    self->limit_lo = (uint16_t)limit;
    self->base_lo = (uint16_t)base;
    self->base_hi = (uint8_t)(base >> 16);
    self->type = flags;
    self->flags_limit_hi = flags_limit_hi;
    self->base_vhi = (uint8_t)(base >> 24);
}

uint32_t jlos_gdt_segment_descriptor_base(jlos_gdt_segment_descriptor_t* self)
{
    return ((uint32_t)self->base_vhi << 24) | ((uint32_t)self->base_hi << 16) | self->base_lo;
}

uint32_t jlos_gdt_segment_descriptor_limit(jlos_gdt_segment_descriptor_t* self)
{
    uint32_t result = (self->flags_limit_hi & JLOS_GDT_FLAGS_LIMIT_HI_MASK);
    result = (result << 16) | self->limit_lo;

    if ((self->flags_limit_hi & JLOS_GDT_FLAGS_GRAN_MASK) == JLOS_GDT_FLAGS_GRAN_MASK) {
        result = (result << 12) | (JLOS_PAGE_SIZE - 1);
    }
    return result;
}

void jlos_mmu_init(void)
{
    jlos_arch_cpu_gdt_init(0);
}

void jlos_arch_cpu_gdt_init(uint32_t cpu)
{
    jlos_mmu_t *gdt = (jlos_mmu_t *)((uint8_t *)&jlos_cpu_gdt + jlos_smp_per_cpu_offset(cpu));

    jlos_gdt_segment_descriptor_init(&gdt->null_segment_selector, 0, 0, 0);
    jlos_gdt_segment_descriptor_init(&gdt->unused_segment_selector, 0, 0, 0);
    jlos_gdt_segment_descriptor_init(&gdt->code_segment_selector, 0, 0xFFFFFFFF, JLOS_GDT_ACCESS_KERNEL_CODE);
    jlos_gdt_segment_descriptor_init(&gdt->data_segment_selector, 0, 0xFFFFFFFF, JLOS_GDT_ACCESS_KERNEL_DATA);
    
    jlos_gdt_segment_descriptor_init(&gdt->user_code_segment_selector, 0, 0xFFFFFFFF, JLOS_GDT_ACCESS_USER_CODE);
    jlos_gdt_segment_descriptor_init(&gdt->user_data_segment_selector, 0, 0xFFFFFFFF, JLOS_GDT_ACCESS_USER_DATA);
#ifdef CONFIG_X86_64
    jlos_gdt_segment_descriptor_init((jlos_gdt_segment_descriptor_t *)&gdt->tss_segment_selector, 0, 0, JLOS_GDT_ACCESS_TSS);
    gdt->tss_segment_selector.base_xhi = 0;
    gdt->tss_segment_selector.reserved = 0;
#else
    jlos_gdt_segment_descriptor_init(&gdt->tss_segment_selector, 0, 0, JLOS_GDT_ACCESS_TSS);
#endif

    jlos_gdt_segment_descriptor_init(&gdt->per_cpu_segment_selector, jlos_smp_per_cpu_offset(cpu), 0xFFFFFFFF, JLOS_GDT_ACCESS_KERNEL_DATA);
#ifdef CONFIG_X86_64
    struct {
        uint16_t size;
        uint64_t base;
    } __attribute__((packed)) idtr;
    idtr.size = sizeof(jlos_mmu_t) - 1;
    idtr.base = (uint64_t)gdt;

    /* 64 位: GS.base 通过 MSR 设置, GDT base 忽略, 不设则 this_cpu_write 写垃圾地址 → #PF */
    uintptr_t gs_base = jlos_smp_per_cpu_offset(cpu);
    uint32_t gs_lo = (uint32_t)(gs_base & 0xFFFFFFFF);
    uint32_t gs_hi = (uint32_t)(gs_base >> 32);

    __asm__ __volatile__(
        "lgdt %0                               \n\t"
        "movw   $" JLOS_X86_ASM_XSTR(JLOS_X86_KERNEL_DS_SEL) ",  %%ax                   \n\t"
        "movw   %%ax,   %%ds                   \n\t"
        "movw   %%ax,   %%es                   \n\t"
        "movw   %%ax,   %%ss                   \n\t"
        "movw   $" JLOS_X86_ASM_XSTR(JLOS_X86_PERCPU_SEL) ",  %%ax                   \n\t"
        "movw   %%ax,   %%gs                   \n\t"
        "movl   $0xC0000101, %%ecx             \n\t"
        "movl   %1, %%eax                       \n\t"
        "movl   %2, %%edx                       \n\t"
        "wrmsr                                   \n\t"
        "leaq   1f(%%rip), %%rax               \n\t"
        "pushq  $" JLOS_X86_ASM_XSTR(JLOS_X86_KERNEL_CS_SEL) "                          \n\t"
        "pushq  %%rax                          \n\t"
        "lretq                                 \n\t"
        "1:                                    \n\t"
        : : "m" (idtr), "r"(gs_lo), "r"(gs_hi) : "rax", "rcx", "rdx", "memory"
    );

#else
    uint32_t i[2];
    i[1] = (uint32_t)gdt;
    i[0] = sizeof(jlos_mmu_t) << 16;

    __asm__ __volatile__(
        "lgdt %0                           \n\t"
        "movw   $" JLOS_X86_ASM_XSTR(JLOS_X86_KERNEL_DS_SEL) ",  %%ax               \n\t"
        "movw   %%ax,   %%ds               \n\t"
        "movw   %%ax,   %%es               \n\t"
        "movw   %%ax,   %%fs               \n\t"
        "movw   $" JLOS_X86_ASM_XSTR(JLOS_X86_PERCPU_SEL) ",  %%ax               \n\t"
        "movw   %%ax,   %%gs               \n\t"
        "movw   $" JLOS_X86_ASM_XSTR(JLOS_X86_KERNEL_DS_SEL) ",  %%ax               \n\t"
        "movw   %%ax,   %%ss               \n\t"
        "ljmp   $" JLOS_X86_ASM_XSTR(JLOS_X86_KERNEL_CS_SEL) ", $1f                 \n\t"
        "1:                                \n\t"
        : : "m" (*(((uint8_t *) i)+2)) : "eax", "memory"
    );
#endif
}

uint16_t jlos_mmu_data_selector(jlos_mmu_t* self)
{
    return (uint8_t *)&self->data_segment_selector - (uint8_t *)self;
}

uint16_t jlos_mmu_code_selector(jlos_mmu_t* self)
{
    return (uint8_t *)&self->code_segment_selector - (uint8_t *)self;
}

uint16_t jlos_mmu_user_code_selector(jlos_mmu_t *self)
{
    return (uint16_t)((uint8_t *)&self->user_code_segment_selector - (uint8_t *)self) | 3;
}

uint16_t jlos_mmu_user_data_selector(jlos_mmu_t *self)
{
    return (uint16_t)((uint8_t *)&self->user_data_segment_selector - (uint8_t *)self) | 3;
}

uint16_t jlos_gdt_tss_selector(jlos_mmu_t *self)
{
    return (uint16_t)((uint8_t *)&self->tss_segment_selector - (uint8_t *)self);
}

void jlos_gdt_set_tss(jlos_mmu_t *self, uintptr_t base, uint32_t limit)
{
    /* 只改内存中的 descriptor，不需要再次 lgdt（GDTR base/limit 没变）。
     * 多余 lgdt 会让一些 CPU 误以为要重刷 descriptor 缓存，
     * 而我们此时 CS/DS 还没加载新 selector，会触发 #GP。
     */
#ifdef CONFIG_X86_64
    jlos_gdt_segment_descriptor_init((jlos_gdt_segment_descriptor_t *)&self->tss_segment_selector, (uint32_t)base, limit, JLOS_GDT_ACCESS_TSS);
    self->tss_segment_selector.base_xhi = (uint32_t)((uint64_t)base >> 32);
    self->tss_segment_selector.reserved = 0;
#else
    jlos_gdt_segment_descriptor_init(&self->tss_segment_selector, (uint32_t)base, limit, JLOS_GDT_ACCESS_TSS);
#endif
}

jlos_mmu_t *jlos_mmu_get_kernel(void)
{
    return this_cpu_ptr(jlos_cpu_gdt);
}
