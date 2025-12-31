#include <kernel/syscalls.h>

namespace JLOS {
namespace Kernel {
void printf(const char *str);

SyscallHandler::SyscallHandler(Hdc::InterruptManager *interruptManager, uint8_t interruptNumber)
: Hdc::InterruptHandler(interruptManager, interruptNumber + interruptManager->HardwareInterruptOffset()){}

SyscallHandler::~SyscallHandler(){}

uint32_t SyscallHandler::HandleInterrupt(uint32_t esp)
{
     CPUState *cpu = (CPUState *)esp;
     switch (cpu->eax) {
          case 4: printf((char *)cpu->ebx); break;
          default: break;
     }
     return esp;
}
}
}