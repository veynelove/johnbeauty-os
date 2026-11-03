/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <hal/rtc.h>
#include <hal/io.h>
#include <hal/paging.h>
#include <common/hardware/fdt.h>
#include <common/types.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "rtc"
#include <kernel/printk.h>

#define JLOS_PL031_REG_DR           0x00
#define JLOS_PL031_NS_PER_SEC       1000000000ULL

static uintptr_t s_pl031_base;
static bool s_pl031_found;

static bool pl031_probe(void)
{
    int32_t node = jlos_fdt_find_node("/pl031");
    if (node < 0) {
        return false;
    }
    const char *compat = (const char *)jlos_fdt_get_prop(node, "compatible", nullptr);
    if (!compat || jlos_strcmp(compat, "arm,pl031") != 0) {
        return false;
    }
    uint32_t len = 0;
    const uint32_t *reg = (const uint32_t *)jlos_fdt_get_prop(node, "reg", &len);
    if (!reg || len < 4 * JLOS_FDT_TOKEN_SIZE) {
        return false;
    }
    s_pl031_base = (uintptr_t)jlos_fdt_read_u64(reg) + KERNEL_DIRECT_MAP_BASE;
    return true;
}

uint64_t jlos_hal_rtc_read_ns(void)
{
    if (!s_pl031_found) {
        s_pl031_found = pl031_probe();
    }
    if (!s_pl031_found) {
        return 0;
    }
    uint32_t seconds = jlos_io_mmio_read32((volatile void *)(s_pl031_base + JLOS_PL031_REG_DR));
    return (uint64_t)seconds * JLOS_PL031_NS_PER_SEC;
}

static void pl031_init(void)
{
    uint64_t ns = jlos_hal_rtc_read_ns();
    if (!s_pl031_found) {
        printk_err("no pl031 rtc in fdt\n");
        return;
    }
    printk_info("rtc: %u seconds since epoch\n", (uint32_t)(ns / JLOS_PL031_NS_PER_SEC));
}

JLOS_INITCALL(JLOS_INITCALL_LATE, pl031_init);
