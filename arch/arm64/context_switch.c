/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <common/types.h>
#include <hal/context.h>
#include <hal/smp.h>
#include <hal/cpu_state.h>
#include <hal/paging.h>
#include <hal/hal_arch.h>
#include <kernel/multitask.h>
#include <kernel/paging.h>
#include <arch/arm64/interrupts.h>
#include <arch/arm64/pt_regs.h>

extern uint8_t __boot_stack_base[];
extern uint8_t __boot_stack_top[];
extern uint8_t __ap_idle_stack_base[];

#define JLOS_ARM64_BOOT_STACK_SIZE  ((uint32_t)(__boot_stack_top - __boot_stack_base))
#define JLOS_ARM64_SWITCH_FRAMES    12
#define JLOS_ARM64_SWITCH_X19       0
#define JLOS_ARM64_SWITCH_X20       1
#define JLOS_ARM64_SWITCH_X21       2
#define JLOS_ARM64_SWITCH_X30       11

DEFINE_PER_CPU(jlos_task_t *, jlos_cpu_current_task);
DEFINE_PER_CPU(jlos_task_t *, jlos_cpu_prev_task);

jlos_task_t *jlos_hal_current_task(void)
{
    return *this_cpu_ptr(jlos_cpu_current_task);
}

void jlos_hal_set_current_task(jlos_task_t *task)
{
    *this_cpu_ptr(jlos_cpu_current_task) = task;
}

void jlos_hal_set_prev_task(jlos_task_t *task)
{
    *this_cpu_ptr(jlos_cpu_prev_task) = task;
}

bool jlos_hal_is_task_in_use(jlos_task_t *task)
{
    if (!task) {
        return false;
    }
    for (uint32_t cpu = 0; cpu < jlos_hal_num_cpus(); cpu++) {
        if (*per_cpu_ptr_cpu(jlos_cpu_current_task, cpu) == task) {
            return true;
        }
        if (*per_cpu_ptr_cpu(jlos_cpu_prev_task, cpu) == task) {
            return true;
        }
    }
    return false;
}

[[noreturn]] void jlos_task_do_exit(void)
{
    jlos_task_t *current_task = jlos_hal_current_task();
    uint32_t flags = jlos_hal_irq_save();
    if (current_task) {
        JLOS_TASK_SET_ZOMBIE(current_task, 0);
    }
    jlos_hal_irq_restore(flags);
    jlos_process_exit(current_task, 0);
    for (;;) {
        jlos_hal_halt();
    }
}

static uint64_t *s_switch_frame_init(uint8_t *stack, uint32_t stack_size)
{
    uint64_t *frame = (uint64_t *)(stack + stack_size) - JLOS_ARM64_SWITCH_FRAMES;
    jlos_memset(frame, 0, JLOS_ARM64_SWITCH_FRAMES * sizeof(uint64_t));
    return frame;
}

void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
                uint8_t *stack, uint32_t stack_size)
{
    (void)mmu;
    jlos_task_t *task = container_of(cpustate, jlos_task_t, cpustate);
    uint64_t *frame = s_switch_frame_init(stack, stack_size);
    frame[JLOS_ARM64_SWITCH_X19] = (uint64_t)(uintptr_t)entrypoint;
    frame[JLOS_ARM64_SWITCH_X30] = (uint64_t)(uintptr_t)jlos_task_entry_stub;
    task->sp.value = (uintptr_t)frame;
}

void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
                uint8_t *stack, uint32_t stack_size, uintptr_t user_stack_top)
{
    (void)mmu;
    jlos_task_t *task = container_of(cpustate, jlos_task_t, cpustate);
    uint64_t *frame = s_switch_frame_init(stack, stack_size);
    frame[JLOS_ARM64_SWITCH_X20] = (uint64_t)(uintptr_t)entrypoint;
    frame[JLOS_ARM64_SWITCH_X21] = (uint64_t)user_stack_top;
    frame[JLOS_ARM64_SWITCH_X30] = (uint64_t)(uintptr_t)jlos_task_user_entry_stub;
    task->sp.value = (uintptr_t)frame;
}

void jlos_arch_task_copy_thread(jlos_task_t *child, const jlos_cpu_state_t *parent_trapframe,
    uint8_t *child_kern_stack, uint32_t child_kern_stack_size, uint32_t child_user_stack)
{
    jlos_arm64_pt_regs_t *tf =
        (jlos_arm64_pt_regs_t *)(child_kern_stack + child_kern_stack_size - sizeof(jlos_arm64_pt_regs_t));
    jlos_memcpy(tf, parent_trapframe, sizeof(jlos_cpu_state_t));
    tf->regs[0] = 0;
    if (child_user_stack != 0) {
        tf->sp = child_user_stack;
    }
    uint64_t *frame = (uint64_t *)tf - JLOS_ARM64_SWITCH_FRAMES;
    frame[JLOS_ARM64_SWITCH_X19] = (uintptr_t)tf;
    frame[JLOS_ARM64_SWITCH_X30] = (uint64_t)(uintptr_t)jlos_task_ret_from_fork_stub;
    child->sp.value = (uintptr_t)frame;
}

void jlos_arch_tss_init(void)
{
}

void jlos_arch_tss_set_ctx(uintptr_t ctx)
{
    (void)ctx;
}

void jlos_arch_boot_stack_info(uint8_t **base, uint32_t *size)
{
    uint32_t cpu = jlos_hal_get_cpu_id();
    *size = JLOS_ARM64_BOOT_STACK_SIZE;
    if (cpu == 0) {
        *base = __boot_stack_base;
    } else {
        *base = __ap_idle_stack_base + cpu * JLOS_ARM64_BOOT_STACK_SIZE;
    }
}

void jlos_arch_exec_return(jlos_paging_context_t *pc, uintptr_t entry, uintptr_t stack_top)
{

    jlos_paging_switch(pc);
    __asm__ __volatile__(
        "msr elr_el1, %0\n\t"
        "msr spsr_el1, %1\n\t"
        "msr sp_el0, %2\n\t"
        "eret"
        : : "r"(entry), "r"((uint64_t)JLOS_ARM64_SPSR_EL0T), "r"(stack_top) : "memory");
    __builtin_unreachable();
}
