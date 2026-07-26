#include <hal/irq.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

void jlos_irq_handler_init(jlos_irq_handler_t *self,
                           jlos_irq_manager_t *mgr, uint8_t irq)
{
    jlos_interrupt_handler_init(self, mgr, irq);
}
void jlos_irq_handler_destroy(jlos_irq_handler_t *self)
{
    jlos_interrupt_handler_destroy(self);
}
uint32_t jlos_irq_handler_handle(jlos_irq_handler_t *self, uint32_t esp)
{
    return jlos_interrupt_handler_handle_interrupt(self, esp);
}

void jlos_irq_manager_init(jlos_irq_manager_t *self, uint16_t offset,
                           jlos_mmu_t *mmu, jlos_task_manager_t *tm)
{
    jlos_interrupt_manager_init(self, offset, mmu, tm);
}
void jlos_irq_manager_destroy(jlos_irq_manager_t *self)
{
    jlos_interrupt_manager_destroy(self);
}
void jlos_irq_manager_activate(jlos_irq_manager_t *self)
{
    jlos_interrupt_manager_activate(self);
}
void jlos_irq_manager_deactivate(jlos_irq_manager_t *self)
{
    jlos_interrupt_manager_deactivate(self);
}
uint32_t jlos_irq_manager_handle(uint8_t irq, uint32_t esp)
{
    return jlos_interrupt_manager_handle_interrupt(irq, esp);
}
uint16_t jlos_irq_manager_hw_offset(jlos_irq_manager_t *self)
{
    return jlos_interrupt_manager_hardware_interrupt_offset(self);
}
uint32_t jlos_irq_manager_do_handle(jlos_irq_manager_t *self,
                                    uint8_t irq, uint32_t esp)
{
    return jlos_interrupt_manager_do_handle_interrupt(self, irq, esp);
}
void jlos_irq_manager_register(jlos_irq_manager_t *self,
                               uint8_t irq, jlos_irq_handler_t *h)
{
    jlos_interrupt_manager_register_handler(self, irq, h);
}

void jlos_irq_ignore_request(void)
{
    jlos_ignore_interrupt_request();
}

#endif
