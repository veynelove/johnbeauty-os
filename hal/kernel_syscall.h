#ifndef __JLOS_HAL_KERNEL_SYSCALL_H
#define __JLOS_HAL_KERNEL_SYSCALL_H

#include <common/types.h>
#include <kernel/syscall.h>

uint32_t jlos_syscall_handler_handle_interrupt(jlos_syscall_handler_t* self, uint32_t ctx);

#endif
