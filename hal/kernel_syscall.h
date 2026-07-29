#ifndef __JLOS_HAL_KERNEL_SYSCALL_H
#define __JLOS_HAL_KERNEL_SYSCALL_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/irq.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/kernel_syscall.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture syscall support not implemented yet"
#endif

/* 内核态 syscall 统一命名：x86/ARM 上层透明 */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_syscall_handler_t  jlos_syscall_t;

  extern void jlos_syscall_init(jlos_syscall_t *self, jlos_irq_manager_t *mgr, uint8_t vector);
  extern void jlos_syscall_destroy(jlos_syscall_t *self);
  extern uint32_t jlos_syscall_handle(jlos_syscall_t *self, uint32_t esp);
#endif

#endif
