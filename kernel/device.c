#include <kernel/device.h>
#include <kernel/printk.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "dev"

uint32_t jlos_device_physical_memory_end = 0;
uint32_t jlos_device_available_ram_bytes = 0;

const multiboot_info_t *jlos_device_multiboot_info = NULL;

void jlos_device_init(const multiboot_info_t *mb)
{
    jlos_device_multiboot_info = mb;
    uint32_t phys_start = KERNEL_MEMORY_PHYSICAL_START;
    uint32_t phys_end = phys_start;
    if (!(mb->flags & MULTIBOOT_INFO_MEM_MAP)) {
        printk_warn("mmap not available, using mem_upper\n");
        phys_end = phys_start + mb->mem_upper * 1024;
        if (phys_end > KERNEL_PHYSICAL_MAX) {
            phys_end = KERNEL_PHYSICAL_MAX;
        }
        jlos_device_physical_memory_end = phys_end;
        jlos_device_available_ram_bytes = mb->mem_upper * 1024;
        return;
    }

    multiboot_mmap_entry_t *entries = (multiboot_mmap_entry_t *)PHYS_TO_VIRT(mb->mmap_addr);
    uint32_t count = mb->mmap_length / sizeof(multiboot_mmap_entry_t);
    
    uint64_t max_end = phys_start;
    uint64_t total_ram = 0;
    for (uint32_t i = 0; i < count; i++) {
        multiboot_mmap_entry_t *e = &entries[i];
        if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
            total_ram += e->length;
            uint64_t end = e->base_addr + e->length;
            if (end > max_end) {
                max_end = end;
            }
        }
    }
    if (max_end > KERNEL_PHYSICAL_MAX) {
        max_end = KERNEL_PHYSICAL_MAX;
    }
    jlos_device_physical_memory_end = (uint32_t)max_end;
    jlos_device_available_ram_bytes = (uint32_t)total_ram;
    printk_info("mmap: %u entries, max ram end = 0x%x, total available = %u KB\n",
        count, (uint32_t)jlos_device_physical_memory_end, (uint32_t)(jlos_device_available_ram_bytes / 1024));
}
