/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_BOOT_INFO_H
#define _JLOS_ARCH_X86_BOOT_INFO_H

#include <common/types.h>
#include <common/multiboot.h>
#include <hal/boot_info.h>

const jlos_hal_boot_info_t *jlos_x86_boot_info_from(const multiboot_info_t *mb);
const multiboot_info_t *jlos_x86_multiboot_info(void);

#endif
