#ifndef __JLOS__KERNEL_MULTITASK_H
#define __JLOS__KERNEL_MULTITASK_H

#include <common/types.h>
#include <kernel/gdt.h>

typedef struct {
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
    uint32_t m_esp;
    uint32_t m_ss;
} __attribute__((packed)) jlos_cpu_state_t;

typedef struct {
    uint8_t stack[4096];
    jlos_cpu_state_t *cpustate;
} jlos_task_t;

typedef struct {
    jlos_task_t *tasks[256];
    int m_num_tasks;
    int m_current_task;
} jlos_task_manager_t;

void jlos_task_init(jlos_task_t* self, jlos_gdt_t *gdt, void (*entrypoint)(void));
void jlos_task_destroy(jlos_task_t* self);

void jlos_task_manager_init(jlos_task_manager_t* self);
void jlos_task_manager_destroy(jlos_task_manager_t* self);
bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task);
jlos_cpu_state_t *jlos_task_manager_schedule(jlos_task_manager_t* self, jlos_cpu_state_t *cpustate);

#endif