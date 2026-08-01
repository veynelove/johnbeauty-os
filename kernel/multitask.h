#ifndef __JLOS__KERNEL_MULTITASK_H
#define __JLOS__KERNEL_MULTITASK_H

#include <common/types.h>
#include <hal/mmu.h>
#include <hal/cpu_state.h>
#include <kernel/paging.h>

#define JLOS_TASK_READY             0
#define JLOS_TASK_RUNNING           1
#define JLOS_TASK_BLOCKED           2
#define JLOS_TASK_TERMINATED        3
#define JLOS_TASK_WAITING           4

#define JLOS_TASK_STACK_SIZE        16384
#define JLOS_TASK_NAME_SIZE         32

#define JLOS_TASK_MLFQ_LEVELS       4
#define JLOS_TASK_MLFQ_AGING_TICKS  200

typedef struct jlos_task_t {
    volatile uint32_t status;
    char name[JLOS_TASK_NAME_SIZE];
    uint8_t *stack;
    uint32_t stack_size;
    uint8_t *user_stack;
    uint32_t user_stack_size;
    jlos_cpu_state_t cpustate;
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t exit_code;
    bool is_user_process;
    jlos_paging_context_t *mm;
    uint32_t wake_tick;
    bool sleeping;
    bool yield;
    int32_t errno;
    uint32_t waiting_pid;
    uint32_t priority;
    uint32_t remain_slice;
    uint32_t default_slice;
    uint32_t last_ready_tick;
    struct jlos_task_t *next_wait;
} jlos_task_t;

typedef struct {
    jlos_task_t *tasks[256];
    int num_tasks;
    int current_task;
    jlos_cpu_state_t main_thread_state;
    bool main_thread_saved;
} jlos_task_manager_t;

void jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name);
void jlos_task_init_user(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name);
void jlos_task_destroy(jlos_task_t* self);

void jlos_task_set_ready(jlos_task_t *t);
void jlos_task_set_running(jlos_task_t *t);
void jlos_task_set_blocked(jlos_task_t *t);
void jlos_task_set_terminated(jlos_task_t *t, uint32_t exit_code);
void jlos_task_set_waiting(jlos_task_t *t, uint32_t pid);

#define JLOS_TASK_SET_READY         jlos_task_set_ready
#define JLOS_TASK_SET_RUNNING       jlos_task_set_running
#define JLOS_TASK_SET_BLOCKED       jlos_task_set_blocked
#define JLOS_TASK_SET_TERMINATED    jlos_task_set_terminated
#define JLOS_TASK_SET_WAITING       jlos_task_set_waiting

void jlos_task_manager_init(jlos_task_manager_t* self);
void jlos_task_manager_destroy(jlos_task_manager_t* self);
bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task);
jlos_cpu_state_t *jlos_task_manager_schedule(jlos_task_manager_t* self, jlos_cpu_state_t *cpustate);
jlos_task_t *jlos_task_manager_curr_task_on_tick(jlos_task_manager_t *self);

jlos_task_t *jlos_process_fork(jlos_task_manager_t *self, jlos_task_t *parent);
int jlos_process_exec(jlos_task_manager_t *self, jlos_task_t *task, void (*entrypoint)(void));
void jlos_process_exit(jlos_task_manager_t *self, jlos_task_t *task, uint32_t exit_code);
#endif
