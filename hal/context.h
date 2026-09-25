#ifndef _JLOS_HAL_CONTEXT_H
#define _JLOS_HAL_CONTEXT_H

#include <common/types.h>
#include <hal/mmu.h>
#include <hal/cpu_state.h>

typedef struct jlos_task jlos_task_t;

extern __attribute__((naked)) void jlos_task_entry_stub(void);
extern __attribute__((naked)) void jlos_task_user_entry_stub(void);
extern __attribute__((naked)) void jlos_task_ret_from_fork_stub(void);

extern void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
                uint8_t *stack, uint32_t stack_size);
extern void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
                uint8_t *stack, uint32_t stack_size, uint32_t user_stack_top);

extern void jlos_arch_task_copy_thread(jlos_task_t *child, const jlos_cpu_state_t *parent_trapframe,
    uint8_t *child_kern_stack, uint32_t child_kern_stack_siize, uint32_t child_user_stack);

extern void jlos_arch_tss_init(void);
extern void jlos_arch_tss_set_ctx(uint32_t ctx);
extern void jlos_arch_boot_stack_info(uint8_t **base, uint32_t *size);

extern void jlos_hal_context_switch(uint32_t *old_sp_ptr, uint32_t new_sp);

#endif
