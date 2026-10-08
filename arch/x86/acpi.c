/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/acpi.h>
#include <arch/x86/smp.h>
#include <arch/x86/fixmap.h>
#include <hal/paging.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "acpi"
#include <kernel/printk.h>

static bool jlos_acpi_signature_match(const uint8_t *sig, const char *expected, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (sig[i] != (uint8_t)expected[i]) {
            return false;
        }
    }
    return true;
}

static bool jlos_acpi_rsdp_checksum_ok(const uint8_t *rsdp)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < JLOS_ACPI_RSDP_CHECKSUM_LEN; i++) {
        sum += rsdp[i];
    }
    return sum == 0;
}

static bool jlos_acpi_sdt_checksum_ok(const jlos_acpi_sdt_header_t *header)
{
    const uint8_t *p = (const uint8_t *)header;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < header->length; i++) {
        sum += p[i];
    }
    return sum == 0;
}

static jlos_acpi_rsdp_t *jlos_acpi_find_rsdp(void)
{
    uint16_t ebda_seg = *(uint16_t *)PHYS_TO_VIRT(JLOS_ACPI_EBDA_PTR_ADDR);
    if (ebda_seg) {
        uint32_t ebda = (uint32_t)ebda_seg << 4;
        for (uint32_t addr = ebda; addr < ebda + JLOS_ACPI_EBDA_SCAN_SIZE; addr += JLOS_ACPI_RSDP_ALIGN) {
            jlos_acpi_rsdp_t *rsdp = (jlos_acpi_rsdp_t *)PHYS_TO_VIRT(addr);
            if (jlos_acpi_signature_match(rsdp->signature, JLOS_ACPI_RSDP_SIGNATURE, 8) && jlos_acpi_rsdp_checksum_ok((const uint8_t *)rsdp)) {
                return rsdp;
            }
        }
    }
    for (uint32_t addr = JLOS_ACPI_RSDP_SCAN_START; addr < JLOS_ACPI_RSDP_SCAN_END; addr += JLOS_ACPI_RSDP_ALIGN) {
        jlos_acpi_rsdp_t *rsdp = (jlos_acpi_rsdp_t *)PHYS_TO_VIRT(addr);
        if (jlos_acpi_signature_match(rsdp->signature, JLOS_ACPI_RSDP_SIGNATURE, 8) && jlos_acpi_rsdp_checksum_ok((const uint8_t *)rsdp)) {
            return rsdp;
        }
    }
    return NULL;
}

static jlos_acpi_madt_t *jlos_acpi_find_madt(jlos_acpi_rsdp_t *rsdp)
{
    uint32_t root_phys;
    uint32_t entry_size;
    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        root_phys = (uint32_t)rsdp->xsdt_address;
        entry_size = 8;
    } else if (rsdp->rsdt_address != 0) {
        root_phys = rsdp->rsdt_address;
        entry_size = 4;
    } else {
        return NULL;
    }

    jlos_acpi_sdt_header_t *root = (jlos_acpi_sdt_header_t *)jlos_fixmap_map(JLOS_FIXMAP_ACPI_0, root_phys);
    if (!jlos_acpi_sdt_checksum_ok(root)) {
        return NULL;
    }
    uint32_t entry_count = (root->length - sizeof(jlos_acpi_sdt_header_t)) / entry_size;
    uint8_t *entries = (uint8_t *)root + sizeof(jlos_acpi_sdt_header_t);
    for (uint32_t i = 0; i < entry_count; i++) {
        uint32_t table_phys;
        if (entry_size == 8) {
            table_phys = (uint32_t)((uint64_t *)entries)[i];
        } else {
            table_phys = ((uint32_t *)entries)[i];
        }
        jlos_acpi_sdt_header_t *sdt = (jlos_acpi_sdt_header_t *)jlos_fixmap_map(JLOS_FIXMAP_ACPI_1, table_phys);
        if (!jlos_acpi_sdt_checksum_ok(sdt)) {
            continue;
        }
        if (jlos_acpi_signature_match(sdt->signature, JLOS_ACPI_MADT_SIGNATURE, 4)) {
            return (jlos_acpi_madt_t *)sdt;
        }
    }
    return NULL;
}

static uint32_t jlos_acpi_parse_madt(jlos_acpi_madt_t *madt)
{
    uint32_t cpu_index = 0;
    uint8_t *entry = (uint8_t *)madt + sizeof(jlos_acpi_madt_t);
    uint8_t *end = (uint8_t *)madt + madt->header.length;
    while (entry < end && cpu_index < JLOS_MAX_CPUS) {
        uint8_t type = entry[0];
        uint8_t length = entry[1];
        if (length == 0) {
            break;
        }
        if (type == JLOS_ACPI_MADT_TYPE_LAPIC && length >= sizeof(jlos_acpi_madt_lapic_t)) {
            jlos_acpi_madt_lapic_t *lapic = (jlos_acpi_madt_lapic_t *)entry;
            if (lapic->flags & JLOS_ACPI_MADT_LAPIC_ENABLED) {
                jlos_arch_smp_set_cpu_apic_id(cpu_index, lapic->apic_id);
                cpu_index++;
            }
        }
        entry += length;
    }
    return cpu_index;
}

void jlos_acpi_init(void)
{
    jlos_acpi_rsdp_t *rsdp = jlos_acpi_find_rsdp();
    if (!rsdp) {
        printk_err("rsdp not found\n");
        return;
    }
    printk_debug("xsdt = 0x%x len = %u oem = %02x%02x%02x%02x%02x%02x ext = 0x%x\n",
        (uint32_t)rsdp->xsdt_address, rsdp->length,
        rsdp->oem_id[0], rsdp->oem_id[1], rsdp->oem_id[2],
        rsdp->oem_id[3], rsdp->oem_id[4], rsdp->oem_id[5],
        rsdp->extended_checksum);
    jlos_acpi_madt_t *madt = jlos_acpi_find_madt(rsdp);
    if (!madt) {
        printk_err("madt not found\n");
        return;
    }
    uint32_t num_cpus = jlos_acpi_parse_madt(madt);
    if (num_cpus == 0) {
        printk_err("no enabled local apic entries\n");
        return;
    }
    jlos_arch_smp_set_num_cpus(num_cpus);
    printk_debug("cpus num = %uu, lapic base = 0x%x\n", num_cpus, madt->lapic_address);
    for (uint32_t i = 0; i < num_cpus; i++) {
        printk_debug("cpu id = %u, apic_id = %u\n", i, jlos_arch_smp_cpu_apic_id(i));
    }
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_acpi_init);
