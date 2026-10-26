/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/boot_info.h>
#include <hal/paging.h>
#include <kernel/memory_manager.h>

static jlos_hal_boot_info_t s_boot_info;
static const multiboot_info_t *s_multiboot;

const jlos_hal_boot_info_t *jlos_x86_boot_info_from(const multiboot_info_t *mb)
{
    s_multiboot = mb;
    jlos_memset(&s_boot_info, 0, sizeof(s_boot_info));
    if (mb->flags & MULTIBOOT_INFO_MEM_MAP) {
        multiboot_mmap_entry_t *entries = (multiboot_mmap_entry_t *)PHYS_TO_VIRT(mb->mmap_addr);
        uint32_t count = mb->mmap_length / sizeof(multiboot_mmap_entry_t);
        for (uint32_t i = 0; i < count && s_boot_info.mem_count < JLOS_HAL_BOOT_MEM_MAX; i++) {
            s_boot_info.mem[s_boot_info.mem_count].addr = entries[i].base_addr;
            s_boot_info.mem[s_boot_info.mem_count].size = entries[i].length;
            s_boot_info.mem[s_boot_info.mem_count].type =
                (entries[i].type == MULTIBOOT_MEMORY_AVAILABLE) ? JLOS_HAL_BOOT_MEM_USABLE : JLOS_HAL_BOOT_MEM_RESERVED;
            s_boot_info.mem_count++;
        }
    } else {
        s_boot_info.mem[0].addr = KERNEL_MEMORY_PHYSICAL_START;
        s_boot_info.mem[0].size = (uint64_t)mb->mem_upper * 1024;
        s_boot_info.mem[0].type = JLOS_HAL_BOOT_MEM_USABLE;
        s_boot_info.mem_count = 1;
    }
    if (mb->flags & MULTIBOOT_INFO_CMDLINE) {
        const char *args = (const char *)PHYS_TO_VIRT(mb->cmdline);
        jlos_strlcpy(s_boot_info.cmdline, args, JLOS_BOOT_CMDLINE_MAX);
    }
    s_boot_info.nr_cpus = 1;
    return &s_boot_info;
}

const multiboot_info_t *jlos_x86_multiboot_info(void)
{
    return s_multiboot;
}
