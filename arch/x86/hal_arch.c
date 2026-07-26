#include <hal/hal.h>

void jlos_hal_halt(void)
{
    __asm__ __volatile__("hlt");
}
