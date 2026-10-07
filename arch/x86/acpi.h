/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_x86_ACPI_H
#define _JLOS_ARCH_x86_ACPI_H

/* Advanced Configuration and Power Interface */

#include <common/types.h>

/* Root System Description Pointer */
typedef struct {
    uint8_t     signature[8];
    uint8_t     checksum;
    uint8_t     oem_id[6];
    uint8_t     revision;
    uint32_t    rsdt_address;
    uint32_t    length;
    uint64_t    xsdt_address;
    uint8_t     extended_checksum;
    uint8_t     reserved[3];
} __attribute__((packed)) jlos_acpi_rsdp_t;

/* Root System Description Table */
typedef struct {
    uint8_t     signature[4];
    uint32_t    length;
    uint8_t     revision;
    uint8_t     checksum;
    uint8_t     oem_id[6];
    uint8_t     oem_table_id[8];
    uint32_t    oem_revision;
    uint32_t    creator_id;
    uint32_t    creator_revision;
} __attribute__((packed)) jlos_acpi_sdt_header_t;

/* Multiple APIC Description Table */
typedef struct {
    jlos_acpi_sdt_header_t  header;
    uint32_t                lapic_address;
    uint32_t                flags;
    uint8_t                 entries[];
} __attribute__((packed)) jlos_acpi_madt_t;

typedef struct {
    uint8_t     type;
    uint8_t     length;
    uint8_t     acpi_processor_id;
    uint8_t     apic_id;
    uint32_t    flags;
} __attribute__((packed)) jlos_acpi_madt_lapic_t;

#define JLOS_ACPI_RSDP_SIGNATURE        "RSD PTR "
#define JLOS_ACPI_RSDP_CHECKSUM_LEN     20
#define JLOS_ACPI_RSDP_SCAN_START       0xe0000
#define JLOS_ACPI_RSDP_SCAN_END         0x100000
#define JLOS_ACPI_RSDP_ALIGN            16

#define JLOS_ACPI_MADT_SIGNATURE        "APIC"
#define JLOS_ACPI_MADT_TYPE_LAPIC       0
#define JLOS_ACPI_MADT_LAPIC_ENABLED    0x1

#define JLOS_ACPI_EBDA_PTR_ADDR         0x40e
#define JLOS_ACPI_EBDA_SCAN_SIZE        0x400

void jlos_acpi_init(void);

#endif
