#ifndef __JLOS_HAL_IRQ_H
#define __JLOS_HAL_IRQ_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/mmu.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/interrupts.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture IRQ support not implemented yet"
#endif

/* IRQ 统一命名：x86 8259+IDT / ARM GIC 上层透明 */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_interrupt_manager_t       jlos_irq_manager_t;
  typedef jlos_interrupt_handler_t       jlos_irq_handler_t;
  typedef jlos_interrupt_handler_func_t  jlos_irq_handler_func_t;

  /* 全局活跃 IRQ 管理器（变量别名用宏，inline 无法表达左值语义） */
#define jlos_active_irq_manager         jlos_active_interrupt_manager

  extern void jlos_irq_handler_init(jlos_irq_handler_t *self,
                                    jlos_irq_manager_t *mgr, uint8_t irq);
  extern void jlos_irq_handler_destroy(jlos_irq_handler_t *self);
  extern uint32_t jlos_irq_handler_handle(uint32_t esp);

  extern void jlos_irq_manager_init(jlos_irq_manager_t *self, uint16_t offset,
                                    jlos_mmu_t *mmu, jlos_task_manager_t *tm);
  extern void jlos_irq_manager_activate(jlos_irq_manager_t *self);
  extern void jlos_irq_manager_deactivate(jlos_irq_manager_t *self);
  extern uint32_t jlos_irq_manager_handle(uint8_t irq, uint32_t esp);
  extern uint16_t jlos_irq_manager_hw_offset(jlos_irq_manager_t *self);
  extern uint32_t jlos_irq_manager_do_handle(jlos_irq_manager_t *self,
                                              uint8_t irq, uint32_t esp);
  extern void jlos_irq_manager_register(jlos_irq_manager_t *self,
                                        uint8_t irq, jlos_irq_handler_t *h);

  extern void jlos_irq_ignore_request(void);
#endif

#define KERNEL_FIRST_INTERRUPT_VECTOR 0x20

typedef struct {
    uint32_t error;
    uint32_t instruction_pointer;
    uint32_t code_segment;
    uint32_t flags;
} jlos_irq_context_t;

void jlos_irq_context_init(jlos_irq_context_t *context, uint32_t arch_state_ptr);
#endif
