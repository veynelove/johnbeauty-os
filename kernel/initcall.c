/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "init"
#include <kernel/printk.h>

extern jlos_initcall_fn_t __initcall0_start, __initcall0_end;
extern jlos_initcall_fn_t __initcall1_start, __initcall1_end;
extern jlos_initcall_fn_t __initcall2_start, __initcall2_end;
extern jlos_initcall_fn_t __initcall3_start, __initcall3_end;
extern jlos_initcall_fn_t __initcall4_start, __initcall4_end;
extern jlos_initcall_fn_t __initcall5_start, __initcall5_end;

static void jlos_run_initcall_level(jlos_initcall_fn_t *start, jlos_initcall_fn_t *end, int level)
{
    (void)level;
    for (jlos_initcall_fn_t *p = start; p < end; p++) {

        (*p)();
    }
}

void jlos_do_initcalls(void)
{
    jlos_run_initcall_level(&__initcall0_start, &__initcall0_end, 0);
    jlos_run_initcall_level(&__initcall1_start, &__initcall1_end, 1);
    jlos_run_initcall_level(&__initcall2_start, &__initcall2_end, 2);
    jlos_run_initcall_level(&__initcall3_start, &__initcall3_end, 3);
    jlos_run_initcall_level(&__initcall4_start, &__initcall4_end, 4);
    jlos_run_initcall_level(&__initcall5_start, &__initcall5_end, 5);
}
