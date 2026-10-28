/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/exception_recover.h>
#include <kernel/initcall.h>
#include <dsa/search.h>
#include <dsa/sort.h>

static int jlos_extable_order_cmp(const void *a, const void *b)
{
    uintptr_t ia = ((const jlos_er_extable_entry_t *)a)->insn;
    uintptr_t ib = ((const jlos_er_extable_entry_t *)b)->insn;
    if (ia < ib) {
        return -1;
    }
    if (ia > ib) {
        return 1;
    }
    return 0;
}

static void jlos_extable_sort_init(void)
{
    jlos_heap_sort(__start_ex_table, JLOS_ER_EXTABLE_SIZE, sizeof(jlos_er_extable_entry_t), jlos_extable_order_cmp);
}

static int jlos_extable_cmp(const void *key, const void *elem)
{
    uintptr_t k = *(const uintptr_t *)key;
    uintptr_t e = ((const jlos_er_extable_entry_t *)elem)->insn;
    if (k < e) {
        return -1;
    }
    if (k > e) {
        return 1;
    }
    return 0;
}

uintptr_t jlos_er_extable_lookup(uintptr_t fault_insn)
{
    jlos_er_extable_entry_t *e = jlos_bsearch(&fault_insn, __start_ex_table, JLOS_ER_EXTABLE_SIZE, sizeof(jlos_er_extable_entry_t), jlos_extable_cmp);
    return e ? e->fixup : 0;
}

JLOS_INITCALL(JLOS_INITCALL_CORE, jlos_extable_sort_init);
