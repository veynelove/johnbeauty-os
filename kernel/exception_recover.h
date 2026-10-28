/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_KERNEL_EXCEPTION_RECOVER_H
#define _JLOS_KERNEL_EXCEPTION_RECOVER_H

#include <common/types.h>

typedef struct {
    uintptr_t insn;
    uintptr_t fixup;
} jlos_er_extable_entry_t;

extern jlos_er_extable_entry_t __start_ex_table[];
extern jlos_er_extable_entry_t __stop_ex_table[];

#define JLOS_ER_EXTABLE_SIZE ((size_t)(__stop_ex_table - __start_ex_table))

uintptr_t jlos_er_extable_lookup(uintptr_t fault_insn);

#endif
