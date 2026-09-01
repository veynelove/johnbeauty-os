#ifndef __JLOS_HAL_EXT_STATE_H
#define __JLOS_HAL_EXT_STATE_H

#include <tools/config.h>
#include <common/types.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/fpu_state.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture ext_state (VFP/ASIMD/SVE) not implemented yet"
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_RISCV
#error "RISC-V ext_state (F/D/V) not implemented yet"
#endif

#define JLOS_ARCH_EXT_STATE_ALIGN   16

typedef struct jlos_arch_ext_state jlos_arch_ext_state_t;
struct jlos_task;
typedef struct jlos_task jlos_task_t;

void jlos_arch_task_ext_init(jlos_task_t *task);
void jlos_arch_task_ext_destroy(jlos_task_t *task);
void jlos_arch_task_ext_switch(void);

void jlos_arch_task_ext_trap_body(void);

#endif
