/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_HAL_SYMMETRIC_MULTI_PROCESSING_H
#define _JLOS_HAL_SYMMETRIC_MULTI_PROCESSING_H

#include <common/types.h>

/* Symmetric Multi-Processing */

#define JLOS_MAX_CPUS   8
#define JLOS_CACHELINE_SIZE     64

#define DEFINE_PER_CPU(type, name) \
    __attribute__((section(".data..percpu"))) \
    __attribute__((aligned(JLOS_CACHELINE_SIZE))) \
    type name

#define DECLARE_PER_CPU(type, name) extern type name

#define this_cpu_ptr(var) \
    ((typeof(var) *)((unsigned long)&(var) + jlos_smp_per_cpu_offset(jlos_hal_get_cpu_id())))

#define per_cpu_ptr_cpu(var, cpu) \
    ((typeof(var) *)((unsigned long)&(var) + jlos_smp_per_cpu_offset(cpu)))

uint32_t jlos_hal_get_cpu_id(void);
uint32_t jlos_hal_num_cpus(void);
uint32_t jlos_smp_per_cpu_offset(uint32_t cpu);

bool jlos_hal_need_resched(void);
void jlos_hal_set_need_resched(bool val);
void jlos_hal_set_need_resched_on(uint32_t cpu, bool val);

void jlos_smp_init(void);
void jlos_hal_smp_boot_aps(void);

#endif
