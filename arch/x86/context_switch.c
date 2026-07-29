#include <hal/context.h>
#include <kernel/multitask.h>
#include <arch/x86/tss.h>

jlos_task_t *g_current_task_ptr = NULL;
uint32_t jlos_arch_tss_base_addr = 0;

static jlos_x86_tss_t s_tss;
static uint8_t s_kernel_stack[4096];

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
        "hlt\n\t"
        "jmp 1b\n\t"
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

void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate,
    jlos_mmu_t *mmu, void (*entrypoint)(void), uint8_t *stack, uint32_t stack_size)
{
    cpustate->m_user_esp = (uint32_t)(stack + stack_size);
    cpustate->m_user_ss = 0;
    cpustate->m_ebx = (uint32_t)entrypoint;
    cpustate->m_edx = (uint32_t)(stack + stack_size);
    cpustate->m_eip = (uint32_t)jlos_task_entry_stub;
    cpustate->m_cs = jlos_mmu_code_selector(mmu);
    cpustate->m_eflags = 0x000;
}

void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
    uint8_t *stack, uint32_t stack_size, uint32_t user_stack_top, uint16_t user_ss)
{
    cpustate->m_user_esp = user_stack_top;
    cpustate->m_user_ss = user_ss;
    cpustate->m_eip = (uint32_t)entrypoint;
    cpustate->m_cs = 0x23;
    cpustate->m_eflags = 0x200;
}

void jlos_arch_tss_init(uint16_t kernel_data_selector)
{
    jlos_gdt_t *gdt = jlos_gdt_get_kernel();
    uint16_t tss_sel = jlos_gdt_tss_selector(gdt);
    jlos_gdt_set_tss(gdt, (uint32_t)&s_tss, sizeof(jlos_x86_tss_t) - 1);
    jlos_x86_tss_init(&s_tss, (uint32_t)(s_kernel_stack + 4096), kernel_data_selector);
    jlos_x86_tss_load(&s_tss, tss_sel);
}

void jlos_arch_tss_set_esp0(uint32_t esp0)
{
    s_tss.m_esp0 = esp0;
}

uint32_t jlos_arch_tss_get_esp0(void)
{
    return s_tss.m_esp0;
}

void jlos_arch_tss_init_for_asm(void)
{
    jlos_arch_tss_base_addr = (uint32_t)&s_tss;
}

