#include <arch/x86/tss.h>
#include <arch/x86/cpu_state.h>
#include <arch/x86/gdt.h>
#include <hal/context.h>
#include <hal/cpu_state.h>
#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <kernel/multitask.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "arch"
#include <kernel/printk.h>

jlos_task_t *g_current_task_ptr = NULL;
uint32_t jlos_arch_tss_base_addr = 0;

static jlos_x86_tss_t s_tss;

extern uint8_t kernel_stack_bottom[];
extern uint8_t kernel_stack[];

__attribute__((noreturn)) void jlos_task_do_exit(void)
{
    __asm__ __volatile__("cli");
    if (g_current_task_ptr) {
        JLOS_TASK_SET_ZOMBIE(g_current_task_ptr, 0);
    }
    __asm__ __volatile__("sti");
    jlos_process_exit(g_current_task_ptr, 0);
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

/* 内核任务首次进入: ebx 由 swtch 帧保存槽填入=entrypoint. 已在私有栈, 无需切栈. */
__attribute__((naked)) void jlos_task_entry_stub(void)
{
    __asm__ __volatile__(
        "xorl %%ebp, %%ebp\n\t"
        "sti\n\t"
        "call *%%ebx\n\t"          /* entrypoint() */
        "jmp jlos_task_exit_stub\n\t"
        ::: "memory");
}

/* 用户任务首次进入: swtch ret 后 esp 正好指向栈顶的 iret frame, 直接 iret 到 ring3. */
__attribute__((naked)) void jlos_task_user_entry_stub(void)
{
    __asm__ __volatile__(
        "movw $" JLOS_X86_ASM_XSTR(JLOS_X86_USER_DS) ", %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "iret\n\t"
        ::: "memory");
}

__attribute__((naked)) void jlos_task_ret_from_fork_stub(void)
{
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
}


void jlos_arch_task_init_arch(jlos_cpu_state_t *cpustate,
    jlos_mmu_t *mmu, void (*entrypoint)(void), uint8_t *stack, uint32_t stack_size)
{
    (void)mmu;
    jlos_task_t *task = container_of(cpustate, jlos_task_t, cpustate);
    /* 栈顶向下布一个 5-dword swtch 帧: [edi][esi][ebx][ebp][ret].
     * swtch 恢复时 pop 4 reg 后 ret 到 entry_stub; ebx 槽 = entrypoint 供 stub 用. */
    uint32_t *top = (uint32_t *)(stack + stack_size);
    top -= 5;
    top[0] = 0;
    top[1] = 0;
    top[2] = (uint32_t)entrypoint;
    top[3] = 0;
    top[4] = (uint32_t)jlos_task_entry_stub;
    
    task->sp.value = (uint32_t)top;
}

void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
    uint8_t *stack, uint32_t stack_size, uint32_t user_stack_top)
{
    (void)mmu;
    jlos_task_t *task = container_of(cpustate, jlos_task_t, cpustate);
    uint32_t *top = (uint32_t *)(stack + stack_size);
    /* iret frame（最顶 5 dword, 供 user_entry_stub iret 到 ring3） */
    top -= 5;
    top[0] = (uint32_t)entrypoint;   /* eip */
    top[1] = JLOS_X86_USER_CS;                   /* cs (user) */
    top[2] = 0x0200;                 /* eflags IF=1 */
    top[3] = user_stack_top;         /* user esp */
    top[4] = JLOS_X86_USER_DS;                /* user ss (0x2B) */

    /* swtch 帧紧贴其下: swtch ret 后 esp 正好落在 iret frame 基址 */
    top -= 5;
    top[0] = 0;
    top[1] = 0;
    top[2] = 0;
    top[3] = 0;
    top[4] = (uint32_t)jlos_task_user_entry_stub;
    
    task->sp.value = (uint32_t)top;
}

void jlos_arch_tss_init(void)
{
    jlos_mmu_t *gdt = jlos_mmu_get_kernel();
    uint16_t tss_sel = jlos_gdt_tss_selector(gdt);
    jlos_gdt_set_tss(gdt, (uint32_t)&s_tss, sizeof(jlos_x86_tss_t) - 1);
    jlos_x86_tss_init(&s_tss, (uint32_t)kernel_stack, jlos_mmu_data_selector(gdt));
    jlos_x86_tss_load(tss_sel);
}

void jlos_arch_tss_set_ctx(uint32_t ctx)
{
    s_tss.esp0 = ctx;
}

void jlos_arch_boot_stack_info(uint8_t **base, uint32_t *size)
{
    *base = kernel_stack_bottom;
    *size = (uint32_t)(kernel_stack - kernel_stack_bottom);
}

void jlos_arch_tss_init_for_asm(void)
{
    jlos_arch_tss_base_addr = (uint32_t)&s_tss;
}

void jlos_arch_task_copy_thread(jlos_task_t *child, const jlos_cpu_state_t *parent_trapframe,
    uint8_t *child_kern_stack, uint32_t child_kern_stack_size, uint32_t child_user_stack)
{
    const jlos_x86_regs_t *pregs = (const jlos_x86_regs_t *)parent_trapframe;
    uint32_t *top = (uint32_t *)(child_kern_stack + child_kern_stack_size);

    /* 完整 trapframe (jlos_x86_regs_t 布局): ret_from_fork_stub pop 全部 GPR + iret */
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


    /* swtch 帧: swtch ret 到 ret_from_fork_stub */
    top -= 5;
    top[0] = 0;
    top[1] = 0;
    top[2] = 0;
    top[3] = 0;
    top[4] = (uint32_t)jlos_task_ret_from_fork_stub;

    child->sp.value = (uint32_t)top;
}

void jlos_arch_exec_return(jlos_paging_context_t *pc, uint32_t entry, uint32_t stack_top)
{
    jlos_paging_switch(pc);
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
}
