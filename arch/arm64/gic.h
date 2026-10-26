/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_GIC_H
#define _JLOS_ARCH_ARM64_GIC_H

#include <common/types.h>

/* GICv3 distributor / redistributor 寄存器偏移 */
#define JLOS_ARCH_GICD_CTLR             0x000
#define JLOS_ARCH_GICD_TYPER            0x004
#define JLOS_ARCH_GICD_IGROUPR(n)       (0x080 + (n) * 4)
#define JLOS_ARCH_GICD_ISENABLER(n)     (0x100 + (n) * 4)
#define JLOS_ARCH_GICD_ICENABLER(n)     (0x180 + (n) * 4)
#define JLOS_ARCH_GICD_ICFGR(n)         (0xC00 + (n) * 4)

#define JLOS_ARCH_GICR_TYPER            0x008
#define JLOS_ARCH_GICR_WAKER            0x014
#define JLOS_ARCH_GICR_SGI_OFF          0x10000
#define JLOS_ARCH_GICR_IGROUPR0         0x080   /* SGI frame 内偏移 */
#define JLOS_ARCH_GICR_ISENABLER0       0x100
#define JLOS_ARCH_GICR_ICENABLER0       0x180
#define JLOS_ARCH_GICR_ICFGR1           0xC04   /* PPI 16-31 触发类型 */
#define JLOS_ARCH_GICR_SGI_ENABLE_MASK  0xFFFF  /* SGI 0-15 使能位 */

/* GICD_CTLR */
#define JLOS_ARCH_GICD_CTLR_ENABLE_G1A  (1U << 0)
#define JLOS_ARCH_GICD_CTLR_ENABLE_G1   (1U << 1)
#define JLOS_ARCH_GICD_CTLR_ARE_NS      (1U << 4)
#define JLOS_ARCH_GICD_CTLR_RWP         (1U << 31)

/* GICR_WAKER */
#define JLOS_ARCH_GICR_WAKER_PROC_SLEEP     (1U << 1)
#define JLOS_ARCH_GICR_WAKER_CHILD_ASLEEP   (1U << 2)

/* GICR_TYPER：affinity 在 [63:32]（Aff0-2 | Aff3 << 24），LAST 在 bit4 */
#define JLOS_ARCH_GICR_TYPER_LAST       (1U << 4)
#define JLOS_ARCH_GICR_TYPER_AFF_SHIFT  32

/* MPIDR：Aff0-2 在 [23:0]，Aff3 在 [39:32]，重打包成与 TYPER.affinity 同构 */
#define JLOS_ARCH_MPIDR_AFF02_MASK      0xFFFFFFULL
#define JLOS_ARCH_MPIDR_AFF3_SHIFT      32
#define JLOS_ARCH_MPIDR_AFF3_MASK       0xFFULL

#define JLOS_ARCH_GICR_STRIDE_DEFAULT   0x20000  /* 2 * 64KB: RD_base + SGI_base */

#define JLOS_ARCH_GIC_ICFGR_EDGE        (1U << 1) /* ICFGR 每中断 2bit，bit1=边沿 */
#define JLOS_ARCH_GIC_INTID_SPURIOUS    1023

bool jlos_arch_gic_ready(void);
void jlos_arch_gic_cpu_init(void);
void jlos_arch_gic_send_sgi(uint32_t intid, uint64_t target_mpidr);
void jlos_arch_gic_irq_unmask(uint32_t intid);
void jlos_arch_gic_irq_mask(uint32_t intid);
void jlos_arch_gic_irq_set_edge(uint32_t intid);
uint32_t jlos_arch_gic_irq_ack(void);
void jlos_arch_gic_irq_eoi(uint32_t intid);

#endif
