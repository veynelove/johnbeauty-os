/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_HAL_KERNEL_SYSCALL_H
#define _JLOS_HAL_KERNEL_SYSCALL_H

#include <common/types.h>
#include <hal/cpu_state.h>

typedef uintptr_t (*jlos_hal_syscall_entry_fn)(void *handler, uintptr_t ctx);
typedef int32_t   (*jlos_hal_syscall_dispatch_fn)(uint32_t num, uintptr_t a1, uintptr_t a2, uintptr_t a3);
typedef bool      (*jlos_hal_syscall_resched_check_fn)(void);
typedef uintptr_t (*jlos_hal_syscall_resched_do_fn)(uintptr_t ctx);

extern jlos_hal_syscall_entry_fn          jlos_hal_syscall_entry;
extern jlos_hal_syscall_dispatch_fn       jlos_hal_syscall_dispatch;
extern jlos_hal_syscall_resched_check_fn  jlos_hal_syscall_resched_check;
extern jlos_hal_syscall_resched_do_fn     jlos_hal_syscall_resched_do;

void jlos_hal_arch_syscall_init(void);

#endif
