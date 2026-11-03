/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_HAL_BOOT_INFO_H
#define _JLOS_HAL_BOOT_INFO_H

#include <common/types.h>

#define JLOS_HAL_BOOT_MEM_MAX       32
#define JLOS_BOOT_CMDLINE_MAX       256

#define JLOS_HAL_BOOT_MEM_USABLE    1
#define JLOS_HAL_BOOT_MEM_RESERVED  2

typedef struct {
    uintptr_t   addr;
    uintptr_t   size;
    uint32_t    type;
} jlos_hal_boot_mem_t;

typedef struct {
    uint32_t            mem_count;
    jlos_hal_boot_mem_t mem[JLOS_HAL_BOOT_MEM_MAX];
    char                cmdline[JLOS_BOOT_CMDLINE_MAX];
    uint32_t            nr_cpus;
    uint32_t            timebase_freq;
} jlos_hal_boot_info_t;

#endif
