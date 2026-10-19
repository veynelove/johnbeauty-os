/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/tss.h>
#include <arch/x86/cpu_state.h>
#include <arch/x86/gdt.h>
#include <arch/x86/smp.h>
#include <hal/context.h>
#include <hal/cpu_state.h>
#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <kernel/multitask.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "arch"
#include <kernel/printk.h>

DEFINE_PER_CPU(jlos_task_t *, jlos_cpu_current_task);
DEFINE_PER_CPU(jlos_task_t *, jlos_cpu_prev_task);
uintptr_t jlos_arch_tss_base_addr = 0;

jlos_task_t *jlos_hal_current_task(void)
{
    return this_cpu_read(jlos_cpu_current_task);
}

void jlos_hal_set_current_task(jlos_task_t *task)
{
    this_cpu_write(jlos_cpu_current_task, task);
}

void jlos_hal_set_prev_task(jlos_task_t *task)
{
    this_cpu_write(jlos_cpu_prev_task, task);
}

bool jlos_hal_is_task_in_use(jlos_task_t *task)
{
    if (!task) {
        return false;
    }
    uint32_t n = jlos_hal_num_cpus();
    for (uint32_t i = 0; i < n; i++) {
        if (*per_cpu_ptr_cpu(jlos_cpu_current_task, i) == task) {
            return true;
        }
        if (*per_cpu_ptr_cpu(jlos_cpu_prev_task, i) == task) {
            return true;
        }
    }
    return false;
}

[[noreturn]] void jlos_task_do_exit(void)
{
    jlos_task_t *current_task = jlos_hal_current_task();
    __asm__ __volatile__("cli");
    if (current_task) {
        JLOS_TASK_SET_ZOMBIE(current_task, 0);
    }
    __asm__ __volatile__("sti");
    jlos_process_exit(current_task, 0);
    for (;;) {
        jlos_hal_halt();
    }
}

__attribute__((naked)) void jlos_task_exit_stub(void)
{
    __asm__ __volatile__(
        "call jlos_task_do_exit\n\t"
        ::: "memory"
    );
}

__attribute__((naked)) void jlos_task_entry_stub(void)
{
#ifdef CONFIG_X86_64
    __asm__ __volatile__(
        "xorq %%rbp, %%rbp\n\t"
        "sti\n\t"
        "call *%%rbx\n\t"
        "jmp jlos_task_exit_stub\n\t"
        ::: "memory");
#else
    __asm__ __volatile__(
        "xorl %%ebp, %%ebp\n\t"
        "sti\n\t"
        "call *%%ebx\n\t"
        "jmp jlos_task_exit_stub\n\t"
        ::: "memory");
#endif
}

__attribute__((naked)) void jlos_task_user_entry_stub(void)
{
#ifdef CONFIG_X86_64
    __asm__ __volatile__(
        "movw $" JLOS_X86_ASM_XSTR(JLOS_X86_USER_DS) ", %%ax\n\t"
        "movw %%ax, %%fs\n\t"

        "iretq\n\t"
        ::: "memory");
#else
    __asm__ __volatile__(
        "movw $" JLOS_X86_ASM_XSTR(JLOS_X86_USER_DS) ", %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "iret\n\t"
        ::: "memory");
#endif
}

__attribute__((naked)) void jlos_task_ret_from_fork_stub(void)
{
#ifdef CONFIG_X86_64
    __asm__ __volatile__(
        "movw $" JLOS_X86_ASM_XSTR(JLOS_X86_USER_DS) ", %%ax\n\t"
        "movw %%ax, %%fs\n\t"

        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
        "popq %%r11\n\t"
        "popq %%r10\n\t"
        "popq %%r9\n\t"
        "popq %%r8\n\t"
        "popq %%rbp\n\t"
        "popq %%rdi\n\t"
        "popq %%rsi\n\t"
        "popq %%rdx\n\t"
        "popq %%rcx\n\t"
        "popq %%rbx\n\t"
        "popq %%rax\n\t"
        "addq $16, %%rsp\n\t"
        "iretq\n\t"
        ::: "memory");
#else
    __asm__ __volatile__(
        "movw $" JLOS_X86_ASM_XSTR(JLOS_X86_USER_DS) ", %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "popl %%ebp\n\t"
        "popl %%edi\n\t"
        "popl %%esi\n\t"
        "popl %%edx\n\t"
        "popl %%ecx\n\t"
        "popl %%ebx\n\t"
        "popl %%eax\n\t"
        "addl $8, %%esp\n\t"
        "iret\n\t"
        ::: "memory");
#endif
}


void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate,
    jlos_mmu_t *mmu, void (*entrypoint)(void), uint8_t *stack, uint32_t stack_size)
{
    (void)mmu;
    jlos_task_t *task = container_of(cpustate, jlos_task_t, cpustate);
#ifdef CONFIG_X86_64
    uint64_t *top = (uint64_t *)(stack + stack_size);
    top -= 7;
    top[0] = 0;
    top[1] = 0;
    top[2] = 0;
    top[3] = 0;
    top[4] = (uint64_t)entrypoint;
    top[5] = 0;
    top[6] = (uint64_t)jlos_task_entry_stub;

    task->sp.value = (uintptr_t)top;
#else
    uint32_t *top = (uint32_t *)(stack + stack_size);
    top -= 5;
    top[0] = 0;
    top[1] = 0;
    top[2] = (uint32_t)entrypoint;
    top[3] = 0;
    top[4] = (uint32_t)jlos_task_entry_stub;

    task->sp.value = (uint32_t)top;
#endif
}

void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
    uint8_t *stack, uint32_t stack_size, uintptr_t user_stack_top)
{
    (void)mmu;
    jlos_task_t *task = container_of(cpustate, jlos_task_t, cpustate);
#ifdef CONFIG_X86_64
    uint64_t *top = (uint64_t *)(stack + stack_size);
    top -= 5;
    top[0] = (uint64_t)entrypoint;
    top[1] = JLOS_X86_USER_CS;
    top[2] = 0x0200;
    top[3] = user_stack_top;
    top[4] = JLOS_X86_USER_DS;

    top -= 7;
    top[0] = 0;
    top[1] = 0;
    top[2] = 0;
    top[3] = 0;
    top[4] = 0;
    top[5] = 0;
    top[6] = (uint64_t)jlos_task_user_entry_stub;

    task->sp.value = (uintptr_t)top;
#else
    uint32_t *top = (uint32_t *)(stack + stack_size);
    top -= 5;
    top[0] = (uint32_t)entrypoint;
    top[1] = JLOS_X86_USER_CS;
    top[2] = 0x0200;
    top[3] = user_stack_top;
    top[4] = JLOS_X86_USER_DS;

    top -= 5;
    top[0] = 0;
    top[1] = 0;
    top[2] = 0;
    top[3] = 0;
    top[4] = (uint32_t)jlos_task_user_entry_stub;

    task->sp.value = (uint32_t)top;
#endif
}

void jlos_arch_tss_init(void)
{
    jlos_mmu_t *gdt = jlos_mmu_get_kernel();
    jlos_x86_tss_t *tss = this_cpu_ptr(jlos_cpu_tss);
    uint16_t tss_sel = jlos_gdt_tss_selector(gdt);
    jlos_gdt_set_tss(gdt, (uintptr_t)tss, sizeof(jlos_x86_tss_t) - 1);
    jlos_x86_tss_init(tss, (uintptr_t)this_cpu_read(jlos_cpu_kernel_stack), jlos_mmu_data_selector(gdt));
    jlos_x86_tss_load(tss_sel);

}

void jlos_arch_tss_set_ctx(uintptr_t ctx)
{
#ifdef CONFIG_X86_64
    this_cpu_ptr(jlos_cpu_tss)->rsp0 = ctx;

#else
    this_cpu_ptr(jlos_cpu_tss)->esp0 = ctx;
#endif
}

void jlos_arch_boot_stack_info(uint8_t **base, uint32_t *size)
{
    *base = this_cpu_read(jlos_cpu_kernel_stack_bottom);
    *size = this_cpu_read(jlos_cpu_kernel_stack_size);
}

void jlos_arch_tss_init_for_asm(void)
{
    jlos_arch_tss_base_addr = (uintptr_t)this_cpu_ptr(jlos_cpu_tss);
}

void jlos_arch_task_copy_thread(jlos_task_t *child, const jlos_cpu_state_t *parent_trapframe,
    uint8_t *child_kern_stack, uint32_t child_kern_stack_size, uint32_t child_user_stack)
{
#ifdef CONFIG_X86_64
    const jlos_x86_regs_t *pregs = (const jlos_x86_regs_t *)parent_trapframe;
    uint64_t *top = (uint64_t *)(child_kern_stack + child_kern_stack_size);

    top -= 22;
    top[0]  = pregs->r15;
    top[1]  = pregs->r14;
    top[2]  = pregs->r13;
    top[3]  = pregs->r12;
    top[4]  = pregs->r11;
    top[5]  = pregs->r10;
    top[6]  = pregs->r9;
    top[7]  = pregs->r8;
    top[8]  = pregs->rbp;
    top[9]  = pregs->rdi;
    top[10] = pregs->rsi;
    top[11] = pregs->rdx;
    top[12] = pregs->rcx;
    top[13] = pregs->rbx;
    top[14] = 0;
    top[15] = 0;
    top[16] = 0;
    top[17] = pregs->eip;
    top[18] = pregs->cs;
    top[19] = pregs->eflags;
    top[20] = (child_user_stack != 0) ? (uint64_t)child_user_stack : pregs->user_esp;
    top[21] = pregs->user_ss;

    top -= 7;
    top[0] = 0;
    top[1] = 0;
    top[2] = 0;
    top[3] = 0;
    top[4] = 0;
    top[5] = 0;
    top[6] = (uint64_t)jlos_task_ret_from_fork_stub;

    child->sp.value = (uintptr_t)top;
#else
    const jlos_x86_regs_t *pregs = (const jlos_x86_regs_t *)parent_trapframe;
    uint32_t *top = (uint32_t *)(child_kern_stack + child_kern_stack_size);

    top -= 14;
    top[0]  = pregs->ebp;
    top[1]  = pregs->edi;
    top[2]  = pregs->esi;
    top[3]  = pregs->edx;
    top[4]  = pregs->ecx;
    top[5]  = pregs->ebx;
    top[6]  = 0;
    top[7]  = 0;
    top[8]  = 0;
    top[9]  = pregs->eip;
    top[10] = pregs->cs;
    top[11] = pregs->eflags;
    top[12] = (child_user_stack != 0) ? child_user_stack : pregs->user_esp;
    top[13] = pregs->user_ss;

    top -= 5;
    top[0] = 0;
    top[1] = 0;
    top[2] = 0;
    top[3] = 0;
    top[4] = (uint32_t)jlos_task_ret_from_fork_stub;

    child->sp.value = (uint32_t)top;
#endif
}

void jlos_arch_exec_return(jlos_paging_context_t *pc, uintptr_t entry, uintptr_t stack_top)
{
    jlos_paging_switch(pc);
#ifdef CONFIG_X86_64

    __asm__ __volatile__(
        "movw %w2, %%ax\n\t"
        "movw %%ax, %%fs\n\t"

        "pushq %2\n\t"
        "pushq %1\n\t"
        "pushq $0x0200\n\t"
        "pushq %3\n\t"
        "pushq %0\n\t"
        "iretq\n\t"
        :: "r"((uint64_t)entry), "r"((uint64_t)stack_top), "i"(JLOS_X86_USER_DS), "i"(JLOS_X86_USER_CS)
        : "rax"
    );
#else
    __asm__ __volatile__(
        "movw %w2, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "pushl %2\n\t"
        "pushl %1\n\t"
        "pushl $0x0200\n\t"
        "pushl %3\n\t"
        "pushl %0\n\t"
        "iret\n\t"
        :: "r"(entry), "r"(stack_top), "i"(JLOS_X86_USER_DS), "i"(JLOS_X86_USER_CS)
        : "eax"
    );
#endif
}
