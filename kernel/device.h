/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_KERNEL_DEVICE_H
#define _JLOS_KERNEL_DEVICE_H

#include <common/types.h>
#include <hal/boot_info.h>

extern uint32_t jlos_device_physical_memory_start;
extern uint32_t jlos_device_physical_memory_end;
extern uint32_t jlos_device_available_ram_bytes;
extern const jlos_hal_boot_info_t *jlos_device_boot_info;

void jlos_device_init(const jlos_hal_boot_info_t *boot_info);

#endif
