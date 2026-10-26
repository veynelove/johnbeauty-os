/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <hal/kernel_syscall.h>

void jlos_hal_arch_init(void)
{
    jlos_hal_info_t *info = jlos_hal_info_get_for_init();

    info->cpu_model     = 0;
    info->cpu_has_cpuid = false;
    info->cpu_has_apic  = false;

    info->irq_mode        = 0;
    info->irq_base_vector = 0;

    info->timer_mode           = JLOS_HAL_TIMER_ARM_GEN;
    info->timer_input_clock_hz = 0;

    info->pci_mmconfig_base   = 0;
    info->pci_ecam_available  = false;

    info->mmio_reserved_start = 0;
    info->mmio_reserved_end   = 0;

    jlos_hal_arch_syscall_init();
}

void jlos_hal_arch_display_init_fb(void)
{
}

void jlos_hal_halt(void)
{
    __asm__ __volatile__("wfi");
}

void jlos_hal_enable_interrupts(void)
{
    __asm__ __volatile__("msr daifclr, #2" ::: "memory");
}

void jlos_hal_disable_interrupts(void)
{
    __asm__ __volatile__("msr daifset, #2" ::: "memory");
}

uint32_t jlos_hal_irq_save(void)
{
    uint32_t daif;
    __asm__ __volatile__("mrs %0, daif" : "=r"(daif));
    __asm__ __volatile__("msr daifset, #2" ::: "memory");
    return daif;
}

void jlos_hal_irq_restore(uint32_t flags)
{
    __asm__ __volatile__("msr daif, %0" :: "r"(flags) : "memory");
}

void jlos_hal_cpu_relax(void)
{
    __asm__ __volatile__("yield" ::: "memory");
}
static bool arm64_pan_supported(void)
{
    uint64_t id;
    __asm__ __volatile__("mrs %0, id_aa64pfr0_el1" : "=r"(id));
    return (((id >> 20) & 0xF) != 0);
}

void jlos_hal_uaccess_begin(void)
{
    if (arm64_pan_supported()) {
        __asm__ __volatile__(".arch_extension pan\n\tmsr pan, #0" ::: "memory");
    }
}

void jlos_hal_uaccess_end(void)
{
    if (arm64_pan_supported()) {
        __asm__ __volatile__(".arch_extension pan\n\tmsr pan, #1" ::: "memory");
    }
}
