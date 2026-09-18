#ifndef _JLOS_HAL_IRQ_H
#define _JLOS_HAL_IRQ_H

#include <common/types.h>

typedef struct jlos_irq_manager jlos_irq_manager_t;
typedef struct jlos_irq_handler jlos_irq_handler_t;

typedef uint32_t (*jlos_irq_handler_func_t)(jlos_irq_handler_t *, uint32_t);

struct jlos_irq_handler {
    uint8_t                   interrupt_number;
    jlos_irq_manager_t        *interrupt_manager;
    jlos_irq_handler_func_t   handle_interrupt;
};

extern jlos_irq_manager_t *jlos_active_irq_manager;

extern void jlos_irq_handler_init(jlos_irq_handler_t *self, jlos_irq_manager_t *mgr, uint8_t irq);
extern uint32_t jlos_irq_handler_handle_interrupt(jlos_irq_handler_t *self, uint32_t esp);

extern void jlos_irq_manager_init(void);
extern void jlos_irq_manager_activate(void);
extern void jlos_irq_manager_deactivate(jlos_irq_manager_t *self);
extern uint32_t jlos_irq_manager_handle(uint8_t irq, uint32_t esp);
extern uint16_t jlos_irq_manager_hw_offset(jlos_irq_manager_t *self);
extern uint32_t jlos_irq_manager_do_handle(jlos_irq_manager_t *self, uint8_t irq, uint32_t esp);
extern void jlos_irq_manager_register(jlos_irq_manager_t *self, uint8_t irq, jlos_irq_handler_t *h);

extern void jlos_irq_ignore_request(void);

typedef struct {
    uint32_t error;
    uint32_t instruction_pointer;
    uint32_t code_segment;
    uint32_t flags;
} jlos_irq_context_t;

extern void jlos_irq_context_init(jlos_irq_context_t *context, uint32_t arch_state_ptr);

#endif
