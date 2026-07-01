#ifndef __JLOS_HAL_CONTEXT_H
#define __JLOS_HAL_CONTEXT_H

#include <tools/config.h>
#include <common/types.h>
#include <kernel/multitask.h>
#include <hal/mmu.h>

/* ============================================================
 *  Task context switch — architecture specific.
 *  Upper layers (kernel/multitask.c) call these functions;
 *  concrete implementations live in arch/<ARCH>/context_switch.c
 *
 *  Implementation notes for porters:
 *    - jlos_task_entry_stub()  —  naked assembly stub that
 *      switches to the task stack and jumps to entrypoint
 *    - jlos_arch_task_init_arch()  —  fills cpustate with the
 *      correct register values (pc/lr/sp, privilege mode, etc.)
 * ============================================================ */

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  extern __attribute__((naked)) void jlos_task_entry_stub(void);
  extern void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate,
                                       jlos_mmu_t      *mmu,
                                       void (*entrypoint)(void),
                                       uint8_t         *stack);
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture task context switch not implemented yet"
#endif

#endif
