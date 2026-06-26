#include <kernel/syscalls.h>
#include <hdc/interrupts.h>

extern void printf(const char *str);

void jlos_syscall_handler_init(jlos_syscall_handler_t* self, jlos_interrupt_manager_t *interrupt_manager, uint8_t m_interrupt_number)
{
    self->m_interrupt_manager = interrupt_manager;
    self->m_interrupt_number = m_interrupt_number;
    jlos_interrupt_handler_init((jlos_interrupt_handler_t*)self, interrupt_manager, m_interrupt_number + jlos_interrupt_manager_hardware_interrupt_offset(interrupt_manager));
}

void jlos_syscall_handler_destroy(jlos_syscall_handler_t* self)
{
}

uint32_t jlos_syscall_handler_handle_interrupt(jlos_syscall_handler_t* self, uint32_t m_esp)
{
    jlos_cpu_state_t *cpu = (jlos_cpu_state_t *)m_esp;
    switch (cpu->m_eax) {
        case 4: printf((char *)cpu->m_ebx); break;
        default: break;
    }
    return m_esp;
}