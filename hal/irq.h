#ifndef __JLOS_HAL_IRQ_H
#define __JLOS_HAL_IRQ_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/mmu.h>
#include <kernel/multitask.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/interrupts.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture IRQ support not implemented yet"
#endif

/* ============================================================
 *  Unified Type Naming (irq = Interrupt Request, generic term)
 * ============================================================ */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_interrupt_manager_t       jlos_irq_manager_t;
  typedef jlos_interrupt_handler_t       jlos_irq_handler_t;
  typedef jlos_interrupt_handler_func_t  jlos_irq_handler_func_t;

  #define jlos_active_irq_manager         jlos_active_interrupt_manager

  #define jlos_irq_handler_init(...)           jlos_interrupt_handler_init(__VA_ARGS__)
  #define jlos_irq_handler_destroy(...)        jlos_interrupt_handler_destroy(__VA_ARGS__)
  #define jlos_irq_handler_handle(...)         jlos_interrupt_handler_handle_interrupt(__VA_ARGS__)

  #define jlos_irq_manager_init(...)           jlos_interrupt_manager_init(__VA_ARGS__)
  #define jlos_irq_manager_destroy(...)        jlos_interrupt_manager_destroy(__VA_ARGS__)
  #define jlos_irq_manager_activate(...)       jlos_interrupt_manager_activate(__VA_ARGS__)
  #define jlos_irq_manager_deactivate(...)     jlos_interrupt_manager_deactivate(__VA_ARGS__)
  #define jlos_irq_manager_handle(...)         jlos_interrupt_manager_handle_interrupt(__VA_ARGS__)
  #define jlos_irq_manager_hw_offset(...)      jlos_interrupt_manager_hardware_interrupt_offset(__VA_ARGS__)
  #define jlos_irq_manager_do_handle(...)      jlos_interrupt_manager_do_handle_interrupt(__VA_ARGS__)
  #define jlos_irq_manager_register(...)       jlos_interrupt_manager_register_handler(__VA_ARGS__)

  #define jlos_irq_ignore_request()            jlos_ignore_interrupt_request()
#endif

/* ============================================================
 *  Unified API Contract
 *
 *  Any architecture port MUST provide:
 *    TYPES:
 *      - jlos_irq_manager_t   : Interrupt controller context
 *      - jlos_irq_handler_t   : Individual IRQ handler descriptor
 *      - jlos_irq_handler_func_t : IRQ handler function pointer type
 *
 *    GLOBALS:
 *      - jlos_irq_manager_t *jlos_active_irq_manager
 *
 *    FUNCTIONS:
 *      - void jlos_irq_manager_init(jlos_irq_manager_t*, uint16_t offset, jlos_mmu_t*, jlos_task_manager_t*)
 *      - void jlos_irq_manager_activate(jlos_irq_manager_t*)
 *      - void jlos_irq_manager_register(jlos_irq_manager_t*, uint8_t irq, jlos_irq_handler_t*)
 * ============================================================ */

#endif
