#ifndef _JLOS_HAL_CLOCK_EVENT_H
#define _JLOS_HAL_CLOCK_EVENT_H

#include <common/types.h>
#include <dsa/list.h>

typedef enum {
    JLOS_CLOCK_EVT_PERIODIC,
    JLOS_CLOCK_EVT_ONESHOT,
    JLOS_CLOCK_EVT_SHUTDOWN,
} jlos_clock_evt_mode_t;

#define JLOS_CLOCK_EVT_FEAT_PERIODIC    0x1
#define JLOS_CLOCK_EVT_FEAT_ONESHOT     0x2

#define JLOS_HAL_TIME_FREQ_HZ   100

typedef struct jlos_clock_event_device {
    const char          *name;
    uint32_t            rating;
    uint32_t            features;
    void                (*set_state_periodic)(uint32_t hz);
    void                (*set_state_oneshot)(void);
    void                (*set_state_shutdown)(void);
    void                (*set_next_event)(uint64_t delta_ns);
    jlos_list_head_t    list;
} jlos_clock_event_device_t;

void jlos_clock_event_register(jlos_clock_event_device_t *dev);
jlos_clock_event_device_t *jlos_clock_event_get_active(void);
void jlos_clock_event_start_periodic(uint32_t hz);
void jlos_clock_event_shutdown(void);

void jlos_clock_event_start_oneshot(void);
void jlos_clock_event_set_next(uint64_t delta_ns);

#endif
