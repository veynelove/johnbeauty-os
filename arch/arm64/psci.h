/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_PSCI_H
#define _JLOS_ARCH_ARM64_PSCI_H

#include <common/types.h>

int32_t jlos_arch_psci_cpu_on(uint64_t mpidr, uintptr_t entry_phys, uintptr_t context);
bool jlos_arch_psci_ready(void);

#endif
