#include <hal/context.h>
#include <kernel/multitask.h>

jlos_task_t *g_current_task_ptr = NULL;

__attribute__((naked)) void jlos_task_exit_stub(void)
{
    __asm__ __volatile__(
        "cli\n\t"
        "movl g_current_task_ptr, %%eax\n\t"
        "test %%eax, %%eax\n\t"
        "jz 1f\n\t"
        "movl %0, (%%eax)\n\t"
    "1:\n\t"
        "sti\n\t"
    "2:\n\t"
        "hlt\n\t"
        "jmp 2b\n\t"
        : : "i"(JLOS_TASK_TERMINATED) : "eax", "memory"
    );
}

/* 任务入口 stub：关中断 → 切私有栈 → 开中断 → 调 entrypoint → 跳 exit stub */
__attribute__((naked)) void jlos_task_entry_stub(void)
{
    __asm__ __volatile__(
        "cli\n\t"
        "mov %%edx, %%esp\n\t"
        "xor %%ebp, %%ebp\n\t"
        "sti\n\t"
        "call *%%ebx\n\t"
        "jmp jlos_task_exit_stub\n\t"
        ::: "memory"
    );
}

void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void), uint8_t *stack)
{
    cpustate->m_ebx = (uint32_t)entrypoint;
    cpustate->m_edx = (uint32_t)(stack + 4096);
    cpustate->m_eip = (uint32_t)jlos_task_entry_stub;
    cpustate->m_cs = jlos_mmu_code_selector(mmu);
}
