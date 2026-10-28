/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/smp.h>
#include <arch/arm64/gic.h>
#include <arch/arm64/interrupts.h>
#include <arch/arm64/psci.h>
#include <common/types.h>
#include <kernel/initcall.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "smp"
#include <kernel/printk.h>

extern uint8_t __per_cpu_start[];
extern uint8_t __per_cpu_end[];
extern uint8_t __ap_boot_stack_base[];
extern void _secondary_start(void);

#define JLOS_ARM64_KIMAGE_PA_OFFSET    0xFFFF800000000000UL
#define JLOS_ARM64_AP_STACK_SIZE       (16 * 1024)
#define JLOS_ARM64_AP_ONLINE_TIMEOUT   10000000U

DEFINE_PER_CPU(uint32_t, jlos_cpu_needs_resched);
DEFINE_PER_CPU(uint32_t, jlos_cpu_online);
DEFINE_PER_CPU(uintptr_t, jlos_cpu_tlb_flush_va);
DEFINE_PER_CPU(uint32_t, jlos_cpu_tlb_flush_done);


static uint32_t  s_num_cpus = 1;
static uintptr_t s_per_cpu_offsets[JLOS_MAX_CPUS];
static uint64_t  s_cpu_mpidrs[JLOS_MAX_CPUS];

static uint64_t arm64_read_mpidr(void)
{
    uint64_t mpidr;
    __asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(mpidr));
    return mpidr;
}

uint32_t jlos_arch_smp_num_cpus(void)
{
    return s_num_cpus;
}

void jlos_arch_smp_set_num_cpus(uint32_t num_cpus)
{
    if (num_cpus > JLOS_MAX_CPUS) {
        num_cpus = JLOS_MAX_CPUS;
    }
    s_num_cpus = num_cpus ? num_cpus : 1;
}

uint64_t jlos_arch_smp_cpu_mpidr(uint32_t cpu)
{
    if (cpu >= s_num_cpus) {
        return 0;
    }
    return s_cpu_mpidrs[cpu];
}

void jlos_arch_smp_set_cpu_mpidr(uint32_t cpu, uint64_t mpidr)
{
    if (cpu >= JLOS_MAX_CPUS) {
        return;
    }
    s_cpu_mpidrs[cpu] = mpidr;
    if (cpu + 1 > s_num_cpus) {
        s_num_cpus = cpu + 1;
    }
}

uint32_t jlos_hal_get_cpu_id(void)
{
    uint64_t mpidr = arm64_read_mpidr();
    for (uint32_t i = 0; i < s_num_cpus; i++) {
        if (s_cpu_mpidrs[i] == mpidr) {
            return i;
        }
    }
    return (uint32_t)(mpidr & 0xFF);
}

uint32_t jlos_hal_num_cpus(void)
{
    return s_num_cpus;
}

uintptr_t jlos_smp_per_cpu_offset(uint32_t cpu)
{
    if (cpu >= JLOS_MAX_CPUS) {
        return 0;
    }
    return s_per_cpu_offsets[cpu];
}

void jlos_smp_init(void)
{
    for (uint32_t i = 0; i < JLOS_MAX_CPUS; i++) {
        s_per_cpu_offsets[i] = 0;
    }
    *this_cpu_ptr(jlos_cpu_needs_resched) = 0;
    *this_cpu_ptr(jlos_cpu_online) = 1;
}

void jlos_smp_alloc_percpu_areas(void)
{
    uint32_t size = (uint32_t)(__per_cpu_end - __per_cpu_start);
    for (uint32_t cpu = 1; cpu < s_num_cpus; cpu++) {
        uint8_t *area = (uint8_t *)jlos_kalloc(size);
        if (!area) {
            printk_err("percpu area alloc failed for cpu %u\n", cpu);
            continue;
        }
        jlos_memset(area, 0, size);
        s_per_cpu_offsets[cpu] = (uintptr_t)area - (uintptr_t)__per_cpu_start;
    }
}

bool jlos_hal_need_resched(void)
{
    return *this_cpu_ptr(jlos_cpu_needs_resched) != 0;
}

void jlos_hal_set_need_resched(bool val)
{
    *this_cpu_ptr(jlos_cpu_needs_resched) = val ? 1 : 0;
}

void jlos_hal_set_need_resched_on(uint32_t cpu, bool val)
{
    uint32_t prev = *per_cpu_ptr_cpu(jlos_cpu_needs_resched, cpu);
    *per_cpu_ptr_cpu(jlos_cpu_needs_resched, cpu) = val ? 1 : 0;
    if (val && !prev && cpu != jlos_hal_get_cpu_id()) {
        jlos_arch_smp_send_ipi(cpu, JLOS_ARCH_ARM64_IPI_RESCHEDULE_SGI);
    }
}

void jlos_arch_smp_send_ipi(uint32_t cpu, uint32_t sgi)
{
    jlos_arch_gic_send_sgi(sgi, jlos_arch_smp_cpu_mpidr(cpu));
}

void jlos_hal_smp_boot_aps(void)
{
    uint32_t num = jlos_hal_num_cpus();
    if (num <= 1) {
        return;
    }
    if (!jlos_arch_psci_ready()) {
        printk_err("no psci, cannot boot aps\n");
        return;
    }
    jlos_smp_alloc_percpu_areas();

    uintptr_t entry_phys = (uintptr_t)(void *)&_secondary_start - JLOS_ARM64_KIMAGE_PA_OFFSET;
    for (uint32_t cpu = 1; cpu < num; cpu++) {
        uintptr_t stack_va = (uintptr_t)(__ap_boot_stack_base + cpu * JLOS_ARM64_AP_STACK_SIZE);
        uint64_t mpidr = jlos_arch_smp_cpu_mpidr(cpu);
        if (jlos_arch_psci_cpu_on(mpidr, entry_phys, stack_va) != 0) {
            printk_err("cpu %u power on failed\n", cpu);
            continue;
        }
        uint32_t spins = 0;
        while (*per_cpu_ptr_cpu(jlos_cpu_online, cpu) == 0 && spins < JLOS_ARM64_AP_ONLINE_TIMEOUT) {
            spins++;
            __asm__ __volatile__("yield");
        }
        if (*per_cpu_ptr_cpu(jlos_cpu_online, cpu) == 0) {
            printk_err("cpu %u online timeout\n", cpu);
        }
    }
    printk_info("%u cpus booted\n", num);
}

JLOS_INITCALL(JLOS_INITCALL_LATE, jlos_hal_smp_boot_aps);
