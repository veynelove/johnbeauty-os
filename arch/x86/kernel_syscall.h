#ifndef __JLOS_ARCH_X86_KERNEL_SYSCALL_H
#define __JLOS_ARCH_X86_KERNEL_SYSCALL_H

#include <common/types.h>
#include <hal/syscall_abi.h>

typedef struct jlos_interrupt_manager jlos_interrupt_manager_t;
typedef struct jlos_syscall_handler jlos_syscall_handler_t;

typedef uint32_t (*jlos_syscall_func_t)(uint32_t arg1, uint32_t arg2, uint32_t arg3);

struct jlos_syscall_handler {
    uint8_t m_interrupt_number;
    jlos_interrupt_manager_t *m_interrupt_manager;
    uint32_t (*handle_interrupt)(jlos_syscall_handler_t*, uint32_t);
    jlos_syscall_func_t m_dispatch[JLOS_SYSCALL_MAX];
};

void jlos_syscall_handler_init(jlos_syscall_handler_t* self, jlos_interrupt_manager_t *interrupt_manager, uint8_t m_interrupt_number);
void jlos_syscall_handler_destroy(jlos_syscall_handler_t* self);
uint32_t jlos_syscall_handler_handle_interrupt(jlos_syscall_handler_t* self, uint32_t m_esp);

#endif
