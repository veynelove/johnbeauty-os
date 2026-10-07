/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/smp.h>
#include <arch/x86/smp.h>

extern uint8_t kernel_stack_bottom[];
extern uint8_t kernel_stack[];

DEFINE_PER_CPU(uint32_t,       jlos_cpu_id);
DEFINE_PER_CPU(uint32_t,       jlos_cpu_lapic_id);
DEFINE_PER_CPU(void *,         jlos_cpu_lapic_base);
DEFINE_PER_CPU(jlos_mmu_t,     jlos_cpu_gdt);
DEFINE_PER_CPU(jlos_x86_tss_t, jlos_cpu_tss);
DEFINE_PER_CPU(void *,         jlos_cpu_kernel_stack);
DEFINE_PER_CPU(void *,         jlos_cpu_kernel_stack_bottom);
DEFINE_PER_CPU(uint32_t,       jlos_cpu_kernel_stack_size);
DEFINE_PER_CPU(bool,           jlos_cpu_online); 

static uint32_t s_per_cpu_offsets[JLOS_MAX_CPUS];
static uint32_t s_num_cpus = 1;
static uint32_t s_cpu_apic_ids[JLOS_MAX_CPUS];

void jlos_smp_init(void)
{
    jlos_memset(s_per_cpu_offsets, 0, sizeof(s_per_cpu_offsets));
    s_per_cpu_offsets[0] = 0;
    
    jlos_cpu_id = 0;
    jlos_cpu_online = true;
    jlos_cpu_kernel_stack = kernel_stack;
    jlos_cpu_kernel_stack_bottom = kernel_stack_bottom;
    jlos_cpu_kernel_stack_size = (uint32_t)(kernel_stack - kernel_stack_bottom);
}

uint32_t jlos_hal_get_cpu_id(void)
{
    return this_cpu_read(jlos_cpu_id);
}

uint32_t jlos_hal_num_cpus(void)
{
    return s_num_cpus;
}

uint32_t jlos_smp_per_cpu_offset(uint32_t cpu)
{
    return s_per_cpu_offsets[cpu];
}

void jlos_arch_smp_set_num_cpus(uint32_t n)
{
    if (n > JLOS_MAX_CPUS) {
        n = JLOS_MAX_CPUS;
    }
    s_num_cpus = n;
}

void jlos_arch_smp_set_cpu_apic_id(uint32_t cpu, uint32_t apic_id)
{
    if (cpu >= JLOS_MAX_CPUS) {
        return;
    }
    s_cpu_apic_ids[cpu] = apic_id;
}

uint32_t jlos_arch_smp_cpu_apic_id(uint32_t cpu)
{
    if (cpu >= JLOS_MAX_CPUS) {
        return 0;
    }
    return s_cpu_apic_ids[cpu];
}
