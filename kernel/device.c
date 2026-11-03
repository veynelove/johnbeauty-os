/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/device.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "dev"
#include <kernel/printk.h>

uint32_t jlos_device_physical_memory_start = 0;
uint32_t jlos_device_physical_memory_end = 0;
uint32_t jlos_device_available_ram_bytes = 0;

const jlos_hal_boot_info_t *jlos_device_boot_info = nullptr;

void jlos_device_init(const jlos_hal_boot_info_t *boot_info)
{
    jlos_device_boot_info = boot_info;
    uintptr_t phys_start = KERNEL_MEMORY_PHYSICAL_START;
    uint64_t min_usable = 0;
    uint64_t max_end = 0;
    uint64_t total_ram = 0;
    for (uint32_t i = 0; i < boot_info->mem_count; i++) {
        const jlos_hal_boot_mem_t *m = &boot_info->mem[i];
        if (m->type != JLOS_HAL_BOOT_MEM_USABLE) {
            continue;
        }       
        if (min_usable == 0 || m->addr < min_usable) {
            min_usable = m->addr;
        }
        total_ram += m->size;
        uint64_t end = m->addr + m->size;
        if (end > max_end) {
            max_end = end;
        }
    }
    if (min_usable) {
        phys_start = (uintptr_t)min_usable;
    }
    if (max_end > KERNEL_PHYSICAL_MAX) {
        max_end = KERNEL_PHYSICAL_MAX;
    }

    jlos_device_physical_memory_start = (uint32_t)phys_start;
    jlos_device_physical_memory_end = (uint32_t)max_end;
    jlos_device_available_ram_bytes = (uint32_t)total_ram;                                                                                                                                                                                  

    printk_info("boot mem: %u entries, start = 0x%x, end = 0x%x, available = %u KB\n",
        boot_info->mem_count, jlos_device_physical_memory_start, jlos_device_physical_memory_end, jlos_device_available_ram_bytes / 1024);
}
