/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_MSR_H
#define _JLOS_ARCH_X86_MSR_H

/* Model Specific Register */

#include <common/types.h>

void jlos_arch_msr_read(uint32_t msr, uint32_t *lo, uint32_t *hi);
void jlos_arch_msr_write(uint32_t msr, uint32_t lo, uint32_t hi);

#endif
