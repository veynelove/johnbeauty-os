/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <common/multiboot.h>
#include <hal/mmu.h>
#include <hal/context.h>
#include <hal/hal_arch.h>
#include <hal/paging.h>
#include <hal/smp.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/device.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "boot"
#include <kernel/printk.h>

void john_beauty_main(const multiboot_info_t *multiboot_structure, uint32_t kernel_end)
{
    jlos_hal_arch_init();
    jlos_printk_init();
    printk_info("princess yihan is safe and happy!\n");

    jlos_smp_init();
    jlos_mmu_init();
    jlos_arch_tss_init();

    jlos_device_init(multiboot_structure);
    jlos_pfa_boot_alloc_init(VIRT_TO_PHYS(kernel_end));
    jlos_arch_paging_initialize_kernel(jlos_pfa_boot_alloc_page);
    jlos_page_frame_allocator_init();
    printk_info("paging initialized\n");

    jlos_do_initcalls();

    for (;;) {
        jlos_hal_halt();
    }
}