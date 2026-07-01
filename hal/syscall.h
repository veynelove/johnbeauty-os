#ifndef __JLOS_HAL_SYSCALL_H
#define __JLOS_HAL_SYSCALL_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/irq.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/syscalls.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture syscall support not implemented yet"
#endif

/* ============================================================
 *  Unified Type Naming (syscall — already generic)
 *  x86: triggers syscall via INT 0x80 instruction
 *  ARM: triggers syscall via SVC #0 instruction
 * ============================================================ */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_syscall_handler_t  jlos_syscall_t;

  #define jlos_syscall_init(...)          jlos_syscall_handler_init(__VA_ARGS__)
  #define jlos_syscall_destroy(...)       jlos_syscall_handler_destroy(__VA_ARGS__)
  #define jlos_syscall_handle(...)        jlos_syscall_handler_handle_interrupt(__VA_ARGS__)
#endif

/* ============================================================
 *  Unified API Contract
 *
 *  Any architecture port MUST provide:
 *    TYPES:
 *      - jlos_syscall_t : Syscall handler context
 *
 *    FUNCTIONS:
 *      - void jlos_syscall_init(jlos_syscall_t*, jlos_irq_manager_t*, uint8_t vector)
 * ============================================================ */

#endif
