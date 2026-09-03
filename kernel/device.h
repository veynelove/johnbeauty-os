#ifndef _JLOS_KERNEL_DEVICE_H
#define _JLOS_KERNEL_DEVICE_H

#include <common/types.h>
#include <common/multiboot.h>

extern uint32_t jlos_device_physical_memory_end;
extern uint32_t jlos_device_available_ram_bytes;
extern const multiboot_info_t *jlos_device_multiboot_info;

void jlos_device_init(const multiboot_info_t *mb);

#endif
