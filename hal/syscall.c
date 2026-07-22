#include <hal/syscall.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

void jlos_syscall_init(jlos_syscall_t *self, jlos_irq_manager_t *mgr, uint8_t vector)
{ jlos_syscall_handler_init(self, mgr, vector); }
void jlos_syscall_destroy(jlos_syscall_t *self)
{ jlos_syscall_handler_destroy(self); }
uint32_t jlos_syscall_handle(jlos_syscall_t *self, uint32_t esp)
{ return jlos_syscall_handler_handle_interrupt(self, esp); }

#endif
