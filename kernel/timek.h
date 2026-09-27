#ifndef _JLOS_KERNEL_TIMEK_H
#define _JLOS_KERNEL_TIMEK_H

#include <common/types.h>

void jlos_timek_init(void);
uint64_t jlos_timek_get_monotonic_ns(void);
uint64_t jlos_timek_get_realtime_ns(void);
void jlos_timek_set_realtime(uint64_t ns);
void jlos_timek_on_tick(void);

uint32_t jlos_timek_get_ticks(void);
void jlos_timek_reset_ticks(void);

#endif
