#ifndef _JLOS_HAL_TIMER_H
#define _JLOS_HAL_TIMER_H

#include <tools/config.h>
#include <common/types.h>

#define JLOS_HAL_TIME_FREQ_HZ 100

/* 统一 timer 驱动：x86 8253 PIT / x86 HPET / ARM Generic Timer 上层透明 */
void jlos_hal_timer_start_periodic(uint16_t freq_hz);

/* 返回自启动以来（或最后一次 reset_ticks 以来）的 tick 数。
 * tick 频率 = 上次 start_periodic() 设置的 freq_hz */
uint32_t jlos_hal_timer_get_ticks(void);

/* 把 tick 计数清 0 */
void jlos_hal_timer_reset_ticks(void);

/* 内部用：PIT / HPET 的 ISR 每次触发时调一次，tick++
 * （多核环境下 future 可以改成 per-CPU 或原子 add） */
void jlos_hal_timer_on_tick(void);

#endif
