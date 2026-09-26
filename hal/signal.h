#ifndef _JLOS_HAL_SIGNAL_H
#define _JLOS_HAL_SIGNAL_H

#include <hal/cpu_state.h>

void jlos_arch_signal_frame_setup(jlos_cpu_state_t *tf, uint32_t sig, uint32_t handler);
void jlos_arch_signal_frame_restore(jlos_cpu_state_t *tf);

#endif
