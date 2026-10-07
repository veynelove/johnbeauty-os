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

uint32_t jlos_hal_get_cpu_id(void);
uint32_t jlos_hal_num_cpus(void);
uint32_t jlos_smp_per_cpu_offset(uint32_t cpu);

void jlos_smp_init(void);

#endif
