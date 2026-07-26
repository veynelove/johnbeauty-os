#ifndef __JLOS__KERNEL_MULTITASK_H
#define __JLOS__KERNEL_MULTITASK_H

#include <common/types.h>
#include <hal/mmu.h>

/* 任务状态 */
#define JLOS_TASK_RUNNING    0
#define JLOS_TASK_TERMINATED 1

typedef struct
{
    uint32_t m_eax;
    uint32_t m_ebx;
    uint32_t m_ecx;
    uint32_t m_edx;
    uint32_t m_esi;
    uint32_t m_edi;
    uint32_t m_ebp;
    uint32_t m_error;
    uint32_t m_eip;
    uint32_t m_cs;
    uint32_t m_eflags;
} __attribute__((packed)) jlos_cpu_state_t;

typedef struct __attribute__((packed)) {
    volatile uint32_t m_status;   /* JLOS_TASK_RUNNING / JLOS_TASK_TERMINATED */
    uint8_t stack[16384];         /* 任务独立栈，16KB，从顶向下生长 */
    jlos_cpu_state_t cpustate;    /* interruptstubs.s SAVE/RESTORE 格式快照 */
    uint32_t m_saved_esp;         /* 栈上 cpustate 地址；0 表示从未被打断 */
} jlos_task_t;

typedef struct {
    jlos_task_t *tasks[256];
    int m_num_tasks;
    int m_current_task;
    jlos_cpu_state_t main_thread_state;  /* 保存主线程的状态 */
    uint32_t main_thread_esp;            /* 保存主线程的 ESP */
    bool main_thread_saved;              /* 是否保存了主线程的状态 */
} jlos_task_manager_t;

void jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void));
void jlos_task_destroy(jlos_task_t* self);

void jlos_task_manager_init(jlos_task_manager_t* self);
void jlos_task_manager_destroy(jlos_task_manager_t* self);
bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task);
jlos_cpu_state_t *jlos_task_manager_schedule(jlos_task_manager_t* self, jlos_cpu_state_t *cpustate);

#endif