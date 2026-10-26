/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_COMMON_HARDWARE_FDT_H
#define _JLOS_COMMON_HARDWARE_FDT_H

#include <common/types.h>

/* Flattened Device Tree */
#define JLOS_FDT_MAGIC  0xD00DFEED

#define JLOS_FDT_BEGIN_NODE 0x1
#define JLOS_FDT_END_NODE   0x2
#define JLOS_FDT_PROP       0x3
#define JLOS_FDT_NOP        0x4
#define JLOS_FDT_END        0x9

#define JLOS_FDT_TOKEN_SIZE         4
#define JLOS_FDT_ALIGN              JLOS_FDT_TOKEN_SIZE
#define JLOS_FDT_PROP_HEADER_SIZE   (3 * JLOS_FDT_TOKEN_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} jlos_fdt_header_t;

int32_t jlos_fdt_init(const void *dtb);
int32_t jlos_fdt_find_node(const char *path);
int32_t jlos_fdt_find_subnode(int32_t parent, const char *name);
int32_t jlos_fdt_first_subnode(int32_t parent);
int32_t jlos_fdt_next_subnode(int32_t node);
const char *jlos_fdt_get_node_name(int32_t node);
const void *jlos_fdt_get_prop(int32_t node, const char *name, uint32_t *len);
bool jlos_fdt_get_prop_u32(int32_t node, const char *name, uint32_t *out);
uint32_t jlos_fdt_read_u32(const void *prop);
uint64_t jlos_fdt_read_u64(const void *prop);
#endif
