/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <common/hardware/fdt.h>

static const uint8_t *s_dtb;
static uint32_t s_struct_off;
static uint32_t s_struct_size;
static uint32_t s_strings_off;

static inline const uint8_t *fdt_addr(uint32_t off)
{
    return &s_dtb[s_struct_off + off];
}

static inline const char *fdt_str(uint32_t off)
{
    return (const char *)&s_dtb[s_strings_off + off];
}

static inline uint32_t fdt_be32(const uint32_t *p)
{
    return __builtin_bswap32(*p);
}                   

static inline uint32_t fdt_tag(uint32_t off)
{
    return fdt_be32((const uint32_t *)fdt_addr(off));
}

static uint32_t fdt_node_name_end(uint32_t node)
{
    const char *name = (const char *)fdt_addr(node + JLOS_FDT_TOKEN_SIZE);
    return JLOS_ALIGN_UP(node + JLOS_FDT_TOKEN_SIZE + jlos_strlen(name) + 1, JLOS_FDT_ALIGN);
}

static bool fdt_node_name_eq(uint32_t node, const char *name, uint32_t len)
{
    const char *n = (const char *)fdt_addr(node + JLOS_FDT_TOKEN_SIZE);
    if (jlos_strncmp(n, name ,len)) {
        return false;
    }
    return n[len] == '\0' || n[len] == '@';
}

static uint32_t fdt_skip_subtree(uint32_t node)
{
    uint32_t off = fdt_node_name_end(node);
    uint32_t depth = 1;
    while (off + JLOS_FDT_TOKEN_SIZE <= s_struct_size) {
        uint32_t tag = fdt_tag(off);
        switch (tag) {
            case JLOS_FDT_BEGIN_NODE:
                depth++;
                off = fdt_node_name_end(off);
                break;
            case JLOS_FDT_END_NODE:
                depth--;
                off += JLOS_FDT_TOKEN_SIZE;
                if (depth == 0) {
                    return off;
                }
                break;
            case JLOS_FDT_PROP: {
                uint32_t len = fdt_be32((const uint32_t *)fdt_addr(off + JLOS_FDT_TOKEN_SIZE));
                off += JLOS_FDT_PROP_HEADER_SIZE + JLOS_ALIGN_UP(len, JLOS_FDT_ALIGN);
                break;
            }
            case JLOS_FDT_NOP:
                off += JLOS_FDT_TOKEN_SIZE;
                break;
            default:
                return s_struct_size;
        }
    }
    return s_struct_size;
}

static uint32_t fdt_first_child(uint32_t parent)
{
    uint32_t off = fdt_node_name_end(parent);
    while (off + JLOS_FDT_TOKEN_SIZE <= s_struct_size) {
        uint32_t tag = fdt_tag(off);
        switch (tag) {
            case JLOS_FDT_BEGIN_NODE:
                return off;
            case JLOS_FDT_END_NODE:
                return 0;
            case JLOS_FDT_PROP: {
                uint32_t len = fdt_be32((const uint32_t *)fdt_addr(off + JLOS_FDT_TOKEN_SIZE));
                off += JLOS_FDT_PROP_HEADER_SIZE + JLOS_ALIGN_UP(len, JLOS_FDT_ALIGN);
                break;
            }
            case JLOS_FDT_NOP:
                off += JLOS_FDT_TOKEN_SIZE;
                break;
            default:
                return 0;
        }
    }
    return 0;
}

static uint32_t fdt_next_sibling(uint32_t node)
{
    uint32_t off = fdt_skip_subtree(node);
    if (off + JLOS_FDT_TOKEN_SIZE <= s_struct_size && fdt_tag(off) == JLOS_FDT_BEGIN_NODE) {
        return off;
    }
    return 0;
}

int32_t jlos_fdt_init(const void *dtb)
{
    const jlos_fdt_header_t *h = (const jlos_fdt_header_t *)dtb;
    if (!dtb || fdt_be32(&h->magic) != JLOS_FDT_MAGIC) {
        return -1;
    }
    uint32_t totalsize = fdt_be32(&h->totalsize);
    uint32_t struct_off = fdt_be32(&h->off_dt_struct);
    uint32_t struct_size = fdt_be32(&h->size_dt_struct);
    uint32_t strings_off = fdt_be32(&h->off_dt_strings);
    if (struct_off == 0 || struct_off >= totalsize || strings_off >= totalsize) {
        return -1;
    }
    if (struct_size == 0) {
        struct_size = totalsize - struct_off;
    }
    if (struct_off + struct_size > totalsize) {
        return -1;
    }
    s_dtb = (const uint8_t *)dtb;
    s_struct_off = struct_off;
    s_struct_size = struct_size;
    s_strings_off = strings_off;
    return 0;
}

int32_t jlos_fdt_find_node(const char *path)
{
    if (!s_dtb || !path || path[0] != '/') {
        return -1;
    }
    uint32_t node = 0;
    const char *p = path;
    while (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        return 0;
    }
    while (*p != '\0') {
        const char *seg = p;
        while (*p != '\0' && *p != '/') {
            p++;
        }
        uint32_t len = (uint32_t)(p - seg);
        uint32_t found = 0;
        for (uint32_t c = fdt_first_child(node); c; c = fdt_next_sibling(c)) {
            if (fdt_node_name_eq(c, seg, len)) {
                found = c;
                break;
            }
        }
        if (found == 0) {
            return -1;
        }
        node = found;
        while (*p == '/') {
            p++;
        } 
    }
    return (int32_t)node;
}

int32_t jlos_fdt_find_subnode(int32_t parent, const char *name)
{
    if (parent < 0 || !name) {
        return -1;
    }
    for (uint32_t c = fdt_first_child((uint32_t)parent); c; c = fdt_next_sibling(c)) {
        if (fdt_node_name_eq(c, name, (uint32_t)jlos_strlen(name))) {
            return (int32_t)c;
        }
    }
    return -1;
}

int32_t jlos_fdt_first_subnode(int32_t parent)
{
    if (parent < 0) {
        return -1;
    }
    uint32_t c = fdt_first_child((uint32_t)parent);
    return c ? (int32_t)c : -1;
}

int32_t jlos_fdt_next_subnode(int32_t node)
{
    if (node < 0) {
        return -1;
    }
    uint32_t n = fdt_next_sibling((uint32_t)node);
    return n ? (int32_t)n : -1;
}

const char *jlos_fdt_get_node_name(int32_t node)
{
    if (node < 0 || !s_dtb) {
        return nullptr;
    }
    return (const char *)fdt_addr((uint32_t)node + JLOS_FDT_TOKEN_SIZE);
}

const void *jlos_fdt_get_prop(int32_t node, const char *name, uint32_t *len)
{
    if (node < 0 || !name) {
        return nullptr;
    }
    uint32_t off = fdt_node_name_end((uint32_t)node);
    while (off + JLOS_FDT_TOKEN_SIZE <= s_struct_size) {
        uint32_t tag = fdt_tag(off);
        switch (tag) {
            case JLOS_FDT_PROP: {
                uint32_t len32 = fdt_be32((const uint32_t *)fdt_addr(off + JLOS_FDT_TOKEN_SIZE));
                uint32_t name_off = fdt_be32((const uint32_t *)fdt_addr(off + 2 * JLOS_FDT_TOKEN_SIZE));
                if (jlos_strcmp(fdt_str(name_off), name) == 0) {
                    if (len) {
                        *len = len32;
                    }
                    return fdt_addr(off + JLOS_FDT_PROP_HEADER_SIZE);
                }
                off += JLOS_FDT_PROP_HEADER_SIZE + JLOS_ALIGN_UP(len32, JLOS_FDT_ALIGN);
                break;
            }
            case JLOS_FDT_NOP:
                off += JLOS_FDT_TOKEN_SIZE;
                break;
            default:
                return nullptr;
        }
    }
    return nullptr;
}

bool jlos_fdt_get_prop_u32(int32_t node, const char *name, uint32_t *out)
{
    const void *p = jlos_fdt_get_prop(node, name, nullptr);
    if (!p) {
        return false;
    }
    if (out) {
        *out = jlos_fdt_read_u32(p);
    }
    return true;
}

uint32_t jlos_fdt_read_u32(const void *prop)
{
    return fdt_be32((const uint32_t *)prop);
}

uint64_t jlos_fdt_read_u64(const void *prop)
{
    const uint32_t *p = (const uint32_t *)prop;
    return ((uint64_t)fdt_be32(p) << 32) | fdt_be32(p + 1);
}
