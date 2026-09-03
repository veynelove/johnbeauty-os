#ifndef _JLOS_HAL_CPU_STATE_H
#define _JLOS_HAL_CPU_STATE_H

#include <common/types.h>   /* uint32_t bool */

#define JLOS_CPU_STATE_SIZE 64

typedef struct {
    uint8_t raw[JLOS_CPU_STATE_SIZE];
} jlos_cpu_state_t;

void jlos_cpu_state_init(jlos_cpu_state_t *s);

bool jlos_cpu_state_is_user_mode(jlos_cpu_state_t *s);
uint32_t jlos_cpu_state_get_syscall_num(jlos_cpu_state_t *s);
void jlos_cpu_state_set_retval(jlos_cpu_state_t *s, int32_t val);
void jlos_cpu_state_record_user_stack(jlos_cpu_state_t *curr_cpu, jlos_cpu_state_t *cpu);

/* per-task saved sp — 经典 Linux task_struct.thread.sp 范式.
 * 调度入口把 on-stack cpustate 指针 (= 真 esp) 存进 task->sp, fork 时
 * 按 delta 平移到子栈. 参数 void* opaque, HAL 不依赖 kernel 头文件. */
typedef struct {
    uint32_t value;
} jlos_arch_sp_t;

extern void     jlos_arch_task_set_sp(void *task, uint32_t real_on_stack_cpustate);
extern uint32_t jlos_arch_task_get_sp(void *task);
extern uint32_t jlos_arch_task_copy_sp(void *parent, void *child,
                                       uint32_t parent_stack_base,
                                       uint32_t child_stack_base);

#endif
