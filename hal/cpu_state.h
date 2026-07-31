#ifndef JLOS_HAL_CPU_STATE_H
#define JLOS_HAL_CPU_STATE_H

#define JLOS_CPU_STATE_SIZE 64

typedef struct {
    uint8_t raw[JLOS_CPU_STATE_SIZE];
} jlos_cpu_state_t;

void jlos_cpu_state_init(jlos_cpu_state_t *s);

bool jlos_cpu_state_is_user_mode(jlos_cpu_state_t *s);
uint32_t jlos_cpu_state_get_syscall_num(jlos_cpu_state_t *s);
void jlos_cpu_state_set_retval(jlos_cpu_state_t *s, int32_t val);
void jlos_cpu_state_record_user_stack(jlos_cpu_state_t *curr_cpu, jlos_cpu_state_t *cpu);

#endif
