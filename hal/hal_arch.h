#ifndef _JLOS_HAL_ARCH_H
#define _JLOS_HAL_ARCH_H

#include <common/types.h>

void jlos_hal_arch_init(void);
void jlos_hal_kernel_segments_init(void);
void jlos_hal_halt(void);
void jlos_hal_enable_interrupts(void);

void jlos_hal_arch_display_register(void);
void jlos_hal_arch_display_init_fb(void);

#endif
