#ifndef _JLOS_HAL_EXT_STATE_H
#define _JLOS_HAL_EXT_STATE_H

#include <common/types.h>

#define JLOS_ARCH_EXT_STATE_SIZE    512
#define JLOS_ARCH_EXT_STATE_ALIGN   16

typedef struct jlos_arch_ext_state {
    uint8_t raw[JLOS_ARCH_EXT_STATE_SIZE] __attribute__((aligned(JLOS_ARCH_EXT_STATE_ALIGN)));
    bool    used;
} jlos_arch_ext_state_t;

typedef struct jlos_task jlos_task_t;

void jlos_arch_task_ext_init(jlos_task_t *task);
void jlos_arch_task_ext_destroy(jlos_task_t *task);
void jlos_arch_task_ext_switch(void);

void jlos_arch_task_ext_trap_body(void);

#endif
