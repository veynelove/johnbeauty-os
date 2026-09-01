#ifndef __JLOS_HAL_CONTEXT_H
#define __JLOS_HAL_CONTEXT_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/mmu.h>
#include <hal/cpu_state.h>

/* Task context switch HAL 接口: kernel 层调用, arch 层实现. HAL 不含汇编. */

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
extern __attribute__((naked)) void jlos_task_entry_stub(void);
extern void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate,
    jlos_mmu_t *mmu, void (*entrypoint)(void), uint8_t *stack, uint32_t stack_size);
extern void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate,
    jlos_mmu_t *mmu, void (*entrypoint)(void), uint8_t *stack, uint32_t stack_size,
    uint32_t user_stack_top, uint16_t user_ss);
/* prepare_child: 高层已做 task+栈 1:1 拷贝, 本函数仅覆写子 cpustate 关键字段
 * (eip=fork_resume_pc/eax=0/cs/eflags/user_esp), 跳过 fork 尾部 return child. */
extern void jlos_arch_task_fork_prepare_child(
    jlos_cpu_state_t *child_cpustate,
    bool is_user_process,
    uint8_t *parent_stack, uint32_t parent_stack_size,
    uint8_t *child_stack,  uint32_t child_stack_size,
    jlos_cpu_state_t *parent_cpustate,
    uint32_t fork_return_pc,    /* fork_invoke 外层 ret addr = __builtin_return_address(0) */
    uint32_t fork_esp_ref,     /* fork_stub asm 首句读硬件 esp (K_DEAD 参考点) */
    uint32_t fork_ebp_ref,     /* fork_stub asm 次句读硬件 ebp */
    uint32_t fork_resume_pc,   /* fork_stub asm 内 call 下一条 label1 */
    void *parent_task,
    void *child_task);
extern void jlos_arch_tss_init(uint16_t kernel_data_selector);
extern void jlos_arch_tss_set_ctx(uint32_t ctx);

/* fork_invoke: 父返回 child* (非 NULL), 子返回 NULL. asm 输出约束 &=a 绑 eax,
 * GCC 不会 spill 父残留. 调用方据此判 is_child = (ret == NULL). */
extern void *jlos_arch_fork_invoke(void *mgr, void *parent);

#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture task context switch not implemented yet"
#endif

#endif
