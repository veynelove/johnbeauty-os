#ifndef _JLOS_HAL_CONTEXT_H
#define _JLOS_HAL_CONTEXT_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/mmu.h>
#include <hal/cpu_state.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

extern __attribute__((naked)) void jlos_task_entry_stub(void);
extern __attribute__((naked)) void jlos_task_user_entry_stub(void);
extern __attribute__((naked)) void jlos_task_fork_entry_stub(void);

extern void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
                uint8_t *stack, uint32_t stack_size);
extern void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
                uint8_t *stack, uint32_t stack_size, uint32_t user_stack_top, uint16_t user_ss);

extern void jlos_arch_task_fork_prepare_child(uint8_t *parent_stack, uint8_t *child_stack, uint32_t fork_esp_ref,
                uint32_t fork_resume_pc, void *child_task);

extern void jlos_arch_tss_init(void);
extern void jlos_arch_tss_set_ctx(uint32_t ctx);
extern void jlos_arch_boot_stack_info(uint8_t **base, uint32_t *size);

extern void *jlos_arch_fork_invoke(void *mgr, void *parent);

extern void jlos_hal_context_switch(uint32_t *old_sp_ptr, uint32_t new_sp);

#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture task context switch not implemented yet"
#endif
#endif
