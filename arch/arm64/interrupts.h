/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_INTERRUPTS_H
#define _JLOS_ARCH_ARM64_INTERRUPTS_H

#include <common/types.h>
#include <arch/arm64/pt_regs.h>
#include <hal/irq.h>
#include <kernel/multitask.h>

#define JLOS_ARCH_GIC_MAX_INTID     1019

#define JLOS_ARCH_ARM64_IPI_RESCHEDULE_SGI  0
#define JLOS_ARCH_ARM64_IPI_TLB_SGI         1

#define JLOS_ARM64_ESR_EC_SVC_A64          0x15
#define JLOS_ARM64_ESR_EC_FP_TRAP          0x07
#define JLOS_ARM64_ESR_EC_INSTR_ABORT_LEL  0x20
#define JLOS_ARM64_ESR_EC_INSTR_ABORT_CEL  0x21
#define JLOS_ARM64_ESR_EC_DATA_ABORT_LEL   0x24
#define JLOS_ARM64_ESR_EC_DATA_ABORT_CEL   0x25

#define JLOS_ARM64_ESR_DFSC_MASK           0x3F
#define JLOS_ARM64_ESR_DFSC_ACCESS_FLAG_L1 0x9
#define JLOS_ARM64_ESR_WNR                 (1U << 6)

typedef struct {
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
    uint64_t esr;
    uint64_t far;
} jlos_arm64_pt_regs_t;

struct jlos_irq_manager {
    void                    *handles[JLOS_ARCH_GIC_MAX_INTID + 1];
    jlos_task_manager_t     *task_manager;
};

void jlos_arm64_irq_dispatch(jlos_arm64_pt_regs_t *frame);
void jlos_arm64_sync_dispatch(jlos_arm64_pt_regs_t *frame);
void jlos_arm64_bad_dispatch(uint32_t kind);

#endif
