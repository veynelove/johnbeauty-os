/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/gic.h>
#include <hal/io.h>
#include <hal/paging.h>
#include <hal/smp.h>
#include <common/hardware/fdt.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "gic"
#include <kernel/printk.h>

#define JLOS_ARCH_GIC_REG_PAIRS         2    /* reg = <dist sz rdist sz> */
#define JLOS_ARCH_GIC_LINE_MAX          1024

static uintptr_t s_dist_base;
static uintptr_t s_rdist_base;
static uint32_t  s_rdist_stride;

DEFINE_PER_CPU(uintptr_t, s_gic_rdist_base);

static uint32_t gicd_read(uint32_t off)
{
    return jlos_io_mmio_read32((volatile void *)(s_dist_base + off));
}

static void gicd_write(uint32_t off, uint32_t val)
{
    jlos_io_mmio_write32((volatile void *)(s_dist_base + off), val);
}

static uint32_t gicr_read(uint32_t off)
{
    return jlos_io_mmio_read32((volatile void *)(*this_cpu_ptr(s_gic_rdist_base) + off));
}

static void gicr_write(uint32_t off, uint32_t val)
{
    jlos_io_mmio_write32((volatile void *)(*this_cpu_ptr(s_gic_rdist_base) + off), val);
}

static void gic_dist_wait_rwp(void)
{
    while (gicd_read(JLOS_ARCH_GICD_CTLR) & JLOS_ARCH_GICD_CTLR_RWP) {
        /* 真硬件 GIC-600 写操作完成前的 RWP 等待 */
    }
}

/* compatible 是 NUL 分隔的多串列表，逐串匹配 */
static bool gic_compatible_match(const char *compat, uint32_t len)
{
    uint32_t off = 0;
    while (off < len) {
        if (jlos_strcmp(compat + off, "arm,gic-v3") == 0) {
            return true;
        }
        while (off < len && compat[off] != '\0') {
            off++;
        }
        off++;
    }
    return false;
}

static int32_t gic_parse_fdt(void)
{
    int32_t node = jlos_fdt_find_node("/intc");
    if (node < 0) {
        printk_err("no /intc node in fdt\n");
        return -1;
    }
    uint32_t len = 0;
    const char *compat = (const char *)jlos_fdt_get_prop(node, "compatible", &len);
    if (!compat || !gic_compatible_match(compat, len)) {
        printk_err("intc is not arm,gic-v3\n");
        return -1;
    }
    int32_t root = jlos_fdt_find_node("/");
    uint32_t addr_cells = 2;
    uint32_t size_cells = 2;
    jlos_fdt_get_prop_u32(root, "#address-cells", &addr_cells);
    jlos_fdt_get_prop_u32(root, "#size-cells", &size_cells);
    if (addr_cells != 2 || size_cells != 2) {
        printk_err("unsupported intc reg cells\n");
        return -1;
    }
    const uint32_t *reg = (const uint32_t *)jlos_fdt_get_prop(node, "reg", &len);
    if (!reg || len < JLOS_ARCH_GIC_REG_PAIRS * 4 * JLOS_FDT_TOKEN_SIZE) {
        printk_err("intc reg invalid\n");
        return -1;
    }
    s_dist_base   = (uintptr_t)jlos_fdt_read_u64(reg) + KERNEL_DIRECT_MAP_BASE;
    s_rdist_base  = (uintptr_t)jlos_fdt_read_u64(reg + 4) + KERNEL_DIRECT_MAP_BASE;
    s_rdist_stride = JLOS_ARCH_GICR_STRIDE_DEFAULT;
    jlos_fdt_get_prop_u32(node, "redistributor-stride", &s_rdist_stride);
    return 0;
}

/* GICR_TYPER.affinity 在 [63:32]；MPIDR 的 Aff0-2 与 Aff3 重打包后整值比较 */
static uintptr_t gic_rdist_find_base(void)
{
    uint64_t mpidr;
    __asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(mpidr));
    uint64_t mpidr_aff = (mpidr & JLOS_ARCH_MPIDR_AFF02_MASK) |
        (((mpidr >> JLOS_ARCH_MPIDR_AFF3_SHIFT) & JLOS_ARCH_MPIDR_AFF3_MASK) << 24);
    uintptr_t base = s_rdist_base;
    for (;;) {
        uint32_t lo = jlos_io_mmio_read32((volatile void *)(base + JLOS_ARCH_GICR_TYPER));
        uint32_t hi = jlos_io_mmio_read32((volatile void *)(base + JLOS_ARCH_GICR_TYPER + 4));
        uint64_t typer = ((uint64_t)hi << 32) | lo;
        uint64_t typer_aff = typer >> JLOS_ARCH_GICR_TYPER_AFF_SHIFT;
        if (typer_aff == mpidr_aff) {
            return base;
        }
        if (typer & JLOS_ARCH_GICR_TYPER_LAST) {
            break;
        }
        base += s_rdist_stride;
    }
    printk_err("no redistributor for mpidr = 0x%lx\n", (unsigned long)mpidr);
    return 0;
}

static void gic_dist_init(void)
{
    gicd_write(JLOS_ARCH_GICD_CTLR, 0);
    gic_dist_wait_rwp();

    uint32_t lines = ((gicd_read(JLOS_ARCH_GICD_TYPER) & 0x1F) + 1) * 32;
    if (lines > JLOS_ARCH_GIC_LINE_MAX) {
        lines = JLOS_ARCH_GIC_LINE_MAX;
    }
    /* SPI 全部归 Group1 并先屏蔽，等驱动注册时逐个 unmask */
    for (uint32_t n = 1; n < lines >> 5; n++) {
        gicd_write(JLOS_ARCH_GICD_IGROUPR(n), ~0U);
        gicd_write(JLOS_ARCH_GICD_ICENABLER(n), ~0U);
    }
    gicd_write(JLOS_ARCH_GICD_CTLR,
        JLOS_ARCH_GICD_CTLR_ENABLE_G1A | JLOS_ARCH_GICD_CTLR_ENABLE_G1 | JLOS_ARCH_GICD_CTLR_ARE_NS);
    gic_dist_wait_rwp();
}

static void gic_rdist_init(void)
{
    uint32_t waker = gicr_read(JLOS_ARCH_GICR_WAKER);
    gicr_write(JLOS_ARCH_GICR_WAKER, waker & ~JLOS_ARCH_GICR_WAKER_PROC_SLEEP);
    while (gicr_read(JLOS_ARCH_GICR_WAKER) & JLOS_ARCH_GICR_WAKER_CHILD_ASLEEP) {
        /* 等待子电源域就绪 */
    }
    /* SGI/PPI 全部归 Group1 */
    gicr_write(JLOS_ARCH_GICR_SGI_OFF + JLOS_ARCH_GICR_IGROUPR0, ~0U);
    gicr_write(JLOS_ARCH_GICR_SGI_OFF + JLOS_ARCH_GICR_ISENABLER0, JLOS_ARCH_GICR_SGI_ENABLE_MASK);
}

static void gic_cpuif_init(void)
{
    uint64_t sre;
    __asm__ __volatile__("mrs %0, icc_sre_el1" : "=r"(sre));
    sre |= 1;
    __asm__ __volatile__("msr icc_sre_el1, %0" : : "r"(sre));
    __asm__ __volatile__("isb");
    __asm__ __volatile__("mrs %0, icc_sre_el1" : "=r"(sre));
    if (!(sre & 1)) {
        printk_err("icc_sre_el1 not set, sysreg access unavailable\n");
        return;
    }
    __asm__ __volatile__("msr icc_pmr_el1, %0" : : "r"((uint64_t)0xFF));
    __asm__ __volatile__("msr icc_igrpen1_el1, %0" : : "r"((uint64_t)1));
    __asm__ __volatile__("isb");
}

void jlos_arch_gic_cpu_init(void)
{
    uintptr_t base = gic_rdist_find_base();
    if (!base) {
        return;
    }
    *this_cpu_ptr(s_gic_rdist_base) = base;

    gic_rdist_init();
    gic_cpuif_init();
}

static void gic_init(void)
{
    if (gic_parse_fdt() < 0) {
        return;
    }
    gic_dist_init();
    jlos_arch_gic_cpu_init();
    printk_info("dist = 0x%lx, rdist = 0x%lx, stride = 0x%x\n",
        (unsigned long)s_dist_base, (unsigned long)*this_cpu_ptr(s_gic_rdist_base), s_rdist_stride);
}

void jlos_arch_gic_irq_unmask(uint32_t intid)
{
    if (intid < 32) {
        gicr_write(JLOS_ARCH_GICR_SGI_OFF + JLOS_ARCH_GICR_ISENABLER0, 1U << intid);
    } else if (intid < JLOS_ARCH_GIC_LINE_MAX) {
        gicd_write(JLOS_ARCH_GICD_ISENABLER(intid >> 5), 1U << (intid & 31));
    }
}

void jlos_arch_gic_irq_mask(uint32_t intid)
{
    if (intid < 32) {
        gicr_write(JLOS_ARCH_GICR_SGI_OFF + JLOS_ARCH_GICR_ICENABLER0, 1U << intid);
    } else if (intid < JLOS_ARCH_GIC_LINE_MAX) {
        gicd_write(JLOS_ARCH_GICD_ICENABLER(intid >> 5), 1U << (intid & 31));
    }
}

void jlos_arch_gic_irq_set_edge(uint32_t intid)
{
    if (intid >= 16 && intid < 32) {
        uint32_t shift = ((intid - 16) << 1) + 1;
        uint32_t reg = gicr_read(JLOS_ARCH_GICR_SGI_OFF + JLOS_ARCH_GICR_ICFGR1);
        gicr_write(JLOS_ARCH_GICR_SGI_OFF + JLOS_ARCH_GICR_ICFGR1, reg | (1U << shift));
    } else if (intid >= 32 && intid < JLOS_ARCH_GIC_LINE_MAX) {
        uint32_t shift = ((intid & 15) << 1) + 1;
        uint32_t reg = gicd_read(JLOS_ARCH_GICD_ICFGR(intid >> 4));
        gicd_write(JLOS_ARCH_GICD_ICFGR(intid >> 4), reg | (1U << shift));
    }
}

uint32_t jlos_arch_gic_irq_ack(void)
{
    uint64_t intid;
    __asm__ __volatile__("mrs %0, icc_iar1_el1" : "=r"(intid));
    return (uint32_t)intid;
}

void jlos_arch_gic_irq_eoi(uint32_t intid)
{
    __asm__ __volatile__("msr icc_eoir1_el1, %0" : : "r"((uint64_t)intid));
}

void jlos_arch_gic_send_sgi(uint32_t intid, uint64_t target_mpidr)
{
    uint64_t aff0 = target_mpidr & 0xFF;
    uint64_t aff1 = (target_mpidr >> 8) & 0xFF;
    uint64_t aff2 = (target_mpidr >> 16) & 0xFF;
    uint64_t aff3 = (target_mpidr >> 32) & 0xFF;
    uint64_t val = (1ULL << aff0)
        | (aff1 << 16)
        | ((uint64_t)(intid & 0xF) << 24)
        | (aff2 << 32)
        | (aff3 << 40);
    __asm__ __volatile__("msr icc_sgi1r_el1, %0" : : "r"(val));
    __asm__ __volatile__("isb");
}

JLOS_INITCALL(JLOS_INITCALL_CORE, gic_init);

bool jlos_arch_gic_ready(void)
{
    return s_dist_base != 0 && *this_cpu_ptr(s_gic_rdist_base) != 0;
}
