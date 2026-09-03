#include <hal/hal.h>

void jlos_hal_halt(void)
{
    __asm__ __volatile__("hlt");
}

void jlos_hal_enable_interrupts(void)
{
    __asm__ __volatile__("sti");
}
