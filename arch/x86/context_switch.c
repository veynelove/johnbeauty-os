#include <arch/x86/tss.h>
#include <arch/x86/cpu_state.h>
#include <arch/x86/gdt.h>
#include <hal/context.h>
#include <hal/cpu_state.h>
#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <kernel/printk.h>
#include <kernel/multitask.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "arch"

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
        "movw $0x2B, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "iret\n\t"
        ::: "memory");
}

__attribute__((naked)) void jlos_task_fork_entry_stub(void)
{
     __asm__ __volatile__(
        "movl $0, %%eax\n\t"
        "movl %%edi, %%esp\n\t"   /* esp = child_label1_esp（6 实参位置） */
        "sti\n\t"
        "jmp *%%ebx\n\t"          /* jmp fork_resume_pc（label1），走真正的 C 尾声 */
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
    uint8_t *stack, uint32_t stack_size, uint32_t user_stack_top, uint16_t user_ss)
{
    (void)mmu;
    jlos_task_t *task = container_of(cpustate, jlos_task_t, cpustate);
    uint32_t *top = (uint32_t *)(stack + stack_size);
    /* iret frame（最顶 5 dword, 供 user_entry_stub iret 到 ring3） */
    top -= 5;
    top[0] = (uint32_t)entrypoint;   /* eip */
    top[1] = 0x23;                   /* cs (user) */
    top[2] = 0x0200;                 /* eflags IF=1 */
    top[3] = user_stack_top;         /* user esp */
    top[4] = user_ss;                /* user ss (0x2B) */

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

void jlos_arch_task_fork_prepare_child(
    uint8_t *parent_stack, uint8_t *child_stack,
    uint32_t fork_esp_ref, uint32_t fork_resume_pc,
    void *child_task)
{
    jlos_task_t *child = (jlos_task_t *)child_task;

    uint32_t delta = (uint32_t)child_stack - (uint32_t)parent_stack;
    /* fork_stub asm 里 fork_esp_ref = 4 次 push 前的真 esp; label1 处 esp = fork_esp_ref - 16 */
    uint32_t child_label1_esp = fork_esp_ref - 16 + delta;

    uint32_t *f = (uint32_t *)(child_label1_esp - 20);
    f[0] = child_label1_esp;        /* edi → fork_entry_stub 切 esp */
    f[1] = 0;                       /* esi */
    f[2] = fork_resume_pc;          /* ebx → fork_entry_stub 跳转 */
    f[3] = 0;                       /* ebp */
    f[4] = (uint32_t)jlos_task_fork_entry_stub;
    child->sp.value = (uint32_t)f;
}
