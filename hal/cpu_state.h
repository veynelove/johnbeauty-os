#ifndef _JLOS_HAL_CPU_STATE_H
#define _JLOS_HAL_CPU_STATE_H

#include <common/types.h>

#define JLOS_CPU_STATE_SIZE 64

typedef struct {
    uint8_t raw[JLOS_CPU_STATE_SIZE];
} jlos_cpu_state_t;

typedef struct {
    uint32_t value;
} jlos_arch_sp_t;

void jlos_cpu_state_init(jlos_cpu_state_t *s);

bool jlos_cpu_state_is_user_mode(jlos_cpu_state_t *s);
uint32_t jlos_cpu_state_get_syscall_num(jlos_cpu_state_t *s);
void jlos_cpu_state_set_retval(jlos_cpu_state_t *s, int32_t val);
void jlos_cpu_state_set_user_entry(jlos_cpu_state_t *s, uint32_t entry, uint32_t stack_top);
void jlos_cpu_state_record_user_stack(jlos_cpu_state_t *curr_cpu, jlos_cpu_state_t *cpu);

#endif
