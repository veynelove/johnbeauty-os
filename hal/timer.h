#ifndef _JLOS_HAL_TIMER_H
#define _JLOS_HAL_TIMER_H

#include <common/types.h>
#include <dsa/list.h>

#define JLOS_HAL_TIME_FREQ_HZ       100

typedef struct jlos_timer_device {
    const char          *name;
    uint32_t            rating;
    void                (*start_periodic)(uint16_t freq_hz);
    jlos_list_head_t    list;
} jlos_timer_device_t;

void jlos_hal_timer_register(jlos_timer_device_t *dev);
jlos_timer_device_t *jlos_hal_timer_get_active(void);

void jlos_hal_timer_start_periodic(uint16_t freq_hz);
uint32_t jlos_hal_timer_get_ticks(void);
void jlos_hal_timer_reset_ticks(void);
void jlos_hal_timer_on_tick(void);

#endif
