#ifndef __JLOS_KERNEL_SYSCALLS_H
#define __JLOS_KERNEL_SYSCALLS_H

#include <common/types.h>

typedef struct jlos_interrupt_manager jlos_interrupt_manager_t;

typedef struct jlos_syscall_handler jlos_syscall_handler_t;

struct jlos_syscall_handler {
    uint8_t m_interrupt_number;
    jlos_interrupt_manager_t *m_interrupt_manager;
    uint32_t (*handle_interrupt)(jlos_syscall_handler_t*, uint32_t);
};

void jlos_syscall_handler_init(jlos_syscall_handler_t* self, jlos_interrupt_manager_t *interrupt_manager, uint8_t m_interrupt_number);
void jlos_syscall_handler_destroy(jlos_syscall_handler_t* self);
uint32_t jlos_syscall_handler_handle_interrupt(jlos_syscall_handler_t* self, uint32_t m_esp);

#endif