#include <hal/timer.h>
#include <hal/io.h>
#include <hal/diag.h>

static volatile uint32_t s_hal_timer_ticks;

void jlos_hal_timer_start_periodic(uint16_t freq_hz)
{
    jlos_io8_slow_t pit_cmd, pit_ch0;
    jlos_io8_slow_init(&pit_cmd, JLOS_HAL_TIMER_CMD_PORT);
    jlos_io8_slow_init(&pit_ch0, JLOS_HAL_TIMER_CH0_PORT);

    uint16_t divisor;
    if (freq_hz == 0) {
        divisor = 0;
    } else {
        divisor = (uint16_t)(JLOS_HAL_TIMER_INPUT_HZ / (uint32_t)freq_hz);
    }

    jlos_io8_slow_write(&pit_cmd, JLOS_HAL_TIMER_CMD_MODE3);
    jlos_io8_slow_write(&pit_ch0, (uint8_t)(divisor & 0xFF));
    jlos_io8_slow_write(&pit_ch0, (uint8_t)((divisor >> 8) & 0xFF));
    s_hal_timer_ticks = 0;
}

uint32_t jlos_hal_timer_get_ticks(void)
{ 
    return s_hal_timer_ticks;
}

void jlos_hal_timer_reset_ticks(void)
{ 
    s_hal_timer_ticks = 0;
}

void jlos_hal_timer_on_tick(void)
{ 
    s_hal_timer_ticks++;
}
