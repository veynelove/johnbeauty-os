#ifndef _JLOS_HAL_KERNEL_SYSCALL_H
#define _JLOS_HAL_KERNEL_SYSCALL_H

#include <common/types.h>
#include <hal/cpu_state.h>

typedef uint32_t (*jlos_hal_syscall_entry_fn)(void *handler, uint32_t ctx);
typedef int32_t  (*jlos_hal_syscall_dispatch_fn)(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3);
typedef bool     (*jlos_hal_syscall_resched_check_fn)(void);
typedef uint32_t (*jlos_hal_syscall_resched_do_fn)(uint32_t ctx);

extern jlos_hal_syscall_entry_fn          jlos_hal_syscall_entry;
extern jlos_hal_syscall_dispatch_fn       jlos_hal_syscall_dispatch;
extern jlos_hal_syscall_resched_check_fn  jlos_hal_syscall_resched_check;
extern jlos_hal_syscall_resched_do_fn     jlos_hal_syscall_resched_do;

extern jlos_cpu_state_t                   *g_hal_syscall_trapframe;

void jlos_hal_arch_syscall_init(void);

#endif
