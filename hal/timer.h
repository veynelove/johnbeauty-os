#ifndef _JLOS_HAL_TIMER_H
#define _JLOS_HAL_TIMER_H

#include <tools/config.h>
#include <common/types.h>

#define JLOS_HAL_TIME_FREQ_HZ       100

#define JLOS_HAL_TIMER_INPUT_HZ     1193180U
#define JLOS_HAL_TIMER_CMD_PORT     0x43
#define JLOS_HAL_TIMER_CH0_PORT     0x40
#define JLOS_HAL_TIMER_CMD_MODE3    0x36

void jlos_hal_timer_start_periodic(uint16_t freq_hz);
uint32_t jlos_hal_timer_get_ticks(void);
void jlos_hal_timer_reset_ticks(void);
void jlos_hal_timer_on_tick(void);

#endif
