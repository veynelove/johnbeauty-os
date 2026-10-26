/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <common/types.h>
#include <hal/boot_info.h>
#include <hal/hal_arch.h>
#include <hal/paging.h>
#include <hal/serial.h>
#include <common/hardware/fdt.h>
#include <arch/arm64/smp.h>

extern char __image_end[];
extern void john_sunshine_main(const jlos_hal_boot_info_t *boot_info, uintptr_t kernel_end);

static jlos_hal_boot_info_t s_boot_info;

static void boot_info_mem_fill(int32_t root)
{
    uint32_t addr_cells = 2;
    uint32_t size_cells = 2;
    jlos_fdt_get_prop_u32(root, "#address-cells", &addr_cells);
    jlos_fdt_get_prop_u32(root, "#size-cells", &size_cells);

    int32_t mem = jlos_fdt_find_node("/memory");
    if (mem < 0) {
        return;
    }
    uint32_t len = 0;
    const uint32_t *reg = (const uint32_t *)jlos_fdt_get_prop(mem, "reg", &len);
    if (!reg) {
        return;
    }
    uint32_t cells = addr_cells + size_cells;
    for (uint32_t off = 0; off + cells <= len / JLOS_FDT_TOKEN_SIZE && s_boot_info.mem_count < JLOS_HAL_BOOT_MEM_MAX; off += cells) {
        const uint32_t *p = &reg[off];
        uintptr_t addr = 0;
        uintptr_t size = 0;
        if (addr_cells == 2 && size_cells == 2) {
            addr = jlos_fdt_read_u64(p);
            size = jlos_fdt_read_u64(p + 2);
        } else if (addr_cells == 1 && size_cells == 1) {
            addr = jlos_fdt_read_u32(p);
            size = jlos_fdt_read_u32(p + 1);
        } else {
            continue;
        }
        s_boot_info.mem[s_boot_info.mem_count].addr = addr;
        s_boot_info.mem[s_boot_info.mem_count].size = size;
        s_boot_info.mem[s_boot_info.mem_count].type = JLOS_HAL_BOOT_MEM_USABLE;
        s_boot_info.mem_count++;
    }
}

static void boot_info_cpus_fill(int32_t cpus)
{
    uint32_t idx = 0;
    for (int32_t c = jlos_fdt_first_subnode(cpus); c >= 0; c = jlos_fdt_next_subnode(c)) {
        uint32_t len = 0;
        const char *type = (const char *)jlos_fdt_get_prop(c, "device_type", &len);
        if (type && len == 4 && jlos_strcmp(type, "cpu") == 0) {
            const uint32_t *reg = (const uint32_t *)jlos_fdt_get_prop(c, "reg", &len);
            uint64_t mpidr = reg ? jlos_fdt_read_u32(reg) : (uint64_t)idx;
            jlos_arch_smp_set_cpu_mpidr(idx, mpidr);
            idx++;
        }
    }
    s_boot_info.nr_cpus = idx;
    jlos_arch_smp_set_num_cpus(idx);
    jlos_fdt_get_prop_u32(cpus, "timebase-frequency", &s_boot_info.timebase_freq);
}


void start_main(uintptr_t dtb_phys)
{
    /* fdt blob 全程经 dmap 访问：纳管分页后无 identity 映射 */
    if (jlos_fdt_init((const void *)PHYS_TO_VIRT(dtb_phys)) < 0) {
        jlos_hal_serial_default_init();
        jlos_hal_serial_default_puts("fdt: invalid dtb, halt\n");
        for (;;) {
            jlos_hal_halt();
        }
    }
    int32_t root = jlos_fdt_find_node("/");
    int32_t cpus = jlos_fdt_find_node("/cpus");
    int32_t chosen = jlos_fdt_find_node("/chosen");
    if (root >= 0) {
        boot_info_mem_fill(root);
    }
    if (cpus >= 0) {
        boot_info_cpus_fill(cpus);
    }
    if (chosen >= 0) {
        const char *args = (const char *)jlos_fdt_get_prop(chosen, "bootargs", nullptr);
        if (args) {
            jlos_strlcpy(s_boot_info.cmdline, args, JLOS_BOOT_CMDLINE_MAX);
        }
    }
    john_sunshine_main(&s_boot_info, (uintptr_t)__image_end);
}
