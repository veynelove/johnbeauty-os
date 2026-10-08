/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_HAL_CLOCKSOURCE_H
#define _JLOS_HAL_CLOCKSOURCE_H

#include <common/types.h>
#include <dsa/list.h>

typedef struct jlos_clocksource
{
    const char          *name;
    uint32_t            rating;
    uint64_t            (*read)(void);
    uint32_t            mult;
    uint32_t            shift;
    uint64_t            mask;
    jlos_list_head_t    list;
} jlos_clocksource_t;

void jlos_clocksource_register(jlos_clocksource_t *cs);
jlos_clocksource_t *jlos_clocksource_get_active(void);
uint64_t jlos_clocksource_read_cycles(void);
uint64_t jlos_clocksource_cycles_to_ns(uint64_t delta);

#endif
