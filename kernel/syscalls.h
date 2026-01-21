#ifndef __JLOS_KERNEL_SYSCALLS_H
#define __JLOS_KERNEL_SYSCALLS_H

#include <hdc/interrupts.h>
#include <kernel/multitask.h>

namespace JLOS {
namespace Kernel {
class syscall_handler : public Hdc::interrupt_handler {
public:
     syscall_handler(Hdc::interrupt_manager *interrupt_manager, uint8_t m_interrupt_number);
     ~syscall_handler();

     uint32_t handle_interrupt(uint32_t m_esp) override;
};
}
}

#endif
