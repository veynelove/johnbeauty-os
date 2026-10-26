/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_SMP_H
#define _JLOS_ARCH_ARM64_SMP_H

#include <common/types.h>
#include <hal/smp.h>

DECLARE_PER_CPU(uint32_t, jlos_cpu_needs_resched);
DECLARE_PER_CPU(uint32_t, jlos_cpu_online);
DECLARE_PER_CPU(uintptr_t, jlos_cpu_tlb_flush_va);
DECLARE_PER_CPU(uint32_t, jlos_cpu_tlb_flush_done);

void jlos_arch_smp_send_ipi(uint32_t cpu, uint32_t sgi);

void jlos_ap_main(void);
void jlos_smp_alloc_percpu_areas(void);

void jlos_arch_smp_set_num_cpus(uint32_t num_cpus);
void jlos_arch_smp_set_cpu_mpidr(uint32_t cpu, uint64_t mpidr);
uint32_t jlos_arch_smp_num_cpus(void);
uint64_t jlos_arch_smp_cpu_mpidr(uint32_t cpu);

#endif