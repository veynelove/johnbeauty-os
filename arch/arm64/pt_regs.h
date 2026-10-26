/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_PT_REGS_H
#define _JLOS_ARCH_ARM64_PT_REGS_H

#define JLOS_ARM64_PT_X(i)          ((i) * 8)
#define JLOS_ARM64_PT_SP_OFF        (31 * 8)
#define JLOS_ARM64_PT_PC_OFF        (32 * 8)
#define JLOS_ARM64_PT_PSTATE_OFF    (33 * 8)
#define JLOS_ARM64_PT_ESR_OFF       (34 * 8)
#define JLOS_ARM64_PT_FAR_OFF       (35 * 8)
#define JLOS_ARM64_PT_SIZE          (36 * 8)

#define JLOS_ARM64_SPSR_EL0T        0

#endif
