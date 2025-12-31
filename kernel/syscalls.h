#ifndef __JLOS_KERNEL_SYSCALLS_H
#define __JLOS_KERNEL_SYSCALLS_H

#include <hdc/interrupts.h>
#include <kernel/multitasking.h>

namespace JLOS {
namespace Kernel {
class SyscallHandler : public Hdc::InterruptHandler {
public:
     SyscallHandler(Hdc::InterruptManager *interruptManager, uint8_t interruptNumber);
     ~SyscallHandler();

     uint32_t HandleInterrupt(uint32_t esp) override;
};
}
}

#endif
