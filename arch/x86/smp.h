/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_SMP_H
#define _JLOS_ARCH_X86_SMP_H

#include <common/types.h>
#include <hal/smp.h>
#include <arch/x86/gdt.h>
#include <arch/x86/tss.h>

DECLARE_PER_CPU(uint32_t,       jlos_cpu_id);
DECLARE_PER_CPU(uint32_t,       jlos_cpu_lapic_id);
DECLARE_PER_CPU(void *,         jlos_cpu_lapic_base);
DECLARE_PER_CPU(jlos_mmu_t,     jlos_cpu_gdt);
DECLARE_PER_CPU(jlos_x86_tss_t, jlos_cpu_tss);
DECLARE_PER_CPU(void *,         jlos_cpu_kernel_stack);
DECLARE_PER_CPU(void *,         jlos_cpu_kernel_stack_bottom);
DECLARE_PER_CPU(uint32_t,       jlos_cpu_kernel_stack_size);
DECLARE_PER_CPU(bool,           jlos_cpu_online); 

#define this_cpu_read(var) \
    ({ typeof(var) _ret; \
       __asm__ __volatile__("mov %%gs:%1, %0" : "=r"(_ret) : "m"(var)); \
       _ret; })

#define this_cpu_write(var, val) \
    do { typeof(var) _val = (val); \
         __asm__ __volatile__("mov %0, %%gs:%1" : : "r"(_val), "m"(var)); \
    } while (0)

#define this_cpu_ptr(var) \
    ((typeof(var) *)((unsigned long)&(var) + jlos_smp_per_cpu_offset(jlos_hal_get_cpu_id())))

#define per_cpu_ptr_cpu(var, cpu) \
    ((typeof(var) *)((unsigned long)&(var) + jlos_smp_per_cpu_offset(cpu)))

#define per_cpu_read_cpu(var, cpu) \
    (*per_cpu_ptr_cpu(var, cpu))

#define per_cpu_write_cpu(var, cpu, val) \
    do { *per_cpu_ptr_cpu(var, cpu) = (val); } while (0)

void jlos_arch_smp_set_num_cpus(uint32_t n);
void jlos_arch_smp_set_cpu_apic_id(uint32_t cpu, uint32_t apic_id);
uint32_t jlos_arch_smp_cpu_apic_id(uint32_t cpu);

void jlos_smp_alloc_percpu_areas(void);

#endif
