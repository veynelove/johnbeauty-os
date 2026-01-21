#include <kernel/syscalls.h>

namespace JLOS {
namespace Kernel {
void printf(const char *str);

syscall_handler::syscall_handler(Hdc::interrupt_manager *interrupt_manager_, uint8_t interrupt_number_)
: Hdc::interrupt_handler(interrupt_manager_, interrupt_number_ + interrupt_manager_->hardware_interrupt_offset()){}

syscall_handler::~syscall_handler(){}

uint32_t syscall_handler::handle_interrupt(uint32_t m_esp)
{
     cpu_state *cpu = (cpu_state *)m_esp;
     switch (cpu->m_eax) {
          case 4: printf((char *)cpu->m_ebx); break;
          default: break;
     }
     return m_esp;
}
}
}