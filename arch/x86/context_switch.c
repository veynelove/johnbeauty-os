#include <arch/x86/tss.h>
#include <arch/x86/cpu_state.h>
#include <hal/context.h>
#include <hal/cpu_state.h>
#include <hal/hal.h>
#include <kernel/printk.h>
#include <kernel/multitask.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "arch"

jlos_task_t *g_current_task_ptr = NULL;
uint32_t jlos_arch_tss_base_addr = 0;

static jlos_x86_tss_t s_tss;

extern uint8_t kernel_stack_bottom[];
extern uint8_t kernel_stack[];

extern void ret_from_fork(void);

void jlos_arch_task_set_sp(void *task, uint32_t real_on_stack_cpustate)
{
    if (!task) return;
    ((jlos_task_t *)task)->sp.value = real_on_stack_cpustate;
}

uint32_t jlos_arch_task_get_sp(void *task)
{
    if (!task) return 0u;
    return ((jlos_task_t *)task)->sp.value;
}

uint32_t jlos_arch_task_copy_sp(void *parent_v, void *child_v,
                                uint32_t p_base, uint32_t c_base)
{
    if (!parent_v || !child_v) return 0u;
    jlos_task_t *p = (jlos_task_t *)parent_v;
    jlos_task_t *c = (jlos_task_t *)child_v;

    uint32_t p_sp  = p->sp.value;
    uint32_t delta = 0;
    uint32_t c_sp  = 0;
    if (p_sp != 0 && p_sp >= p_base && p_sp - p_base < p->stack_size) {
        /* sp 在 [p_stack, +size) 合法段内 → 正常平移 */
        delta = p_sp - p_base;
        c_sp  = c_base + delta;
    } else {
        /* 兜底 (user task 首次 fork 没走过 schedule() 存 sp, 或 idle task).
         * 退回到 "栈顶对齐" 保守策略: sp = stack+size - sizeof(regs_48B),
         * 避免 0 / 非法值造成 ring0_task_return 切错栈. */
        delta = (c->stack_size >= 64)? (c->stack_size - 64) : 0;
        c_sp  = c_base + delta;
    }
    c->sp.value = c_sp;
    return c_sp;
}

__attribute__((noreturn)) void jlos_task_do_exit(void)
{
    __asm__ __volatile__("cli");
    if (g_current_task_ptr) {
        JLOS_TASK_SET_ZOMBIE(g_current_task_ptr, 0);
    }
    __asm__ __volatile__("sti");
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

    ((jlos_x86_regs_t *)cpustate)->user_esp = (uint32_t)(stack + stack_size);
    ((jlos_x86_regs_t *)cpustate)->user_ss = 0;
    ((jlos_x86_regs_t *)cpustate)->ebx = (uint32_t)entrypoint;
    ((jlos_x86_regs_t *)cpustate)->edx = (uint32_t)(stack + stack_size);
    ((jlos_x86_regs_t *)cpustate)->eip = (uint32_t)jlos_task_entry_stub;
    ((jlos_x86_regs_t *)cpustate)->cs = jlos_mmu_code_selector(mmu);
    ((jlos_x86_regs_t *)cpustate)->eflags = 0x000;
}

void jlos_arch_task_init_arch_user(jlos_cpu_state_t *cpustate, jlos_mmu_t *mmu, void (*entrypoint)(void),
    uint8_t *stack, uint32_t stack_size, uint32_t user_stack_top, uint16_t user_ss)
{
    (void)stack;
    (void)stack_size;
    ((jlos_x86_regs_t *)cpustate)->user_esp = user_stack_top;
    ((jlos_x86_regs_t *)cpustate)->user_ss = user_ss;
    ((jlos_x86_regs_t *)cpustate)->eip = (uint32_t)entrypoint;
    ((jlos_x86_regs_t *)cpustate)->cs = 0x23;
    ((jlos_x86_regs_t *)cpustate)->eflags = 0x0200;  /* IF=1, IOPL=0 */
    (void)mmu;
}

void jlos_arch_tss_init(uint16_t kernel_data_selector)
{
    jlos_gdt_t *gdt = jlos_gdt_get_kernel();
    uint16_t tss_sel = jlos_gdt_tss_selector(gdt);
    jlos_gdt_set_tss(gdt, (uint32_t)&s_tss, sizeof(jlos_x86_tss_t) - 1);
    jlos_x86_tss_init(&s_tss, (uint32_t)kernel_stack, kernel_data_selector);
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
    jlos_cpu_state_t *child_cpustate,
    bool is_user_process,
    uint8_t *parent_stack, uint32_t parent_stack_size,
    uint8_t *child_stack,  uint32_t child_stack_size,
    jlos_cpu_state_t *parent_cpustate,
    uint32_t fork_return_pc,
    uint32_t fork_esp_ref,
    uint32_t fork_ebp_ref,
    uint32_t fork_resume_pc,
    void *parent_task, void *child_task)
{
    (void)parent_stack_size;
    jlos_x86_regs_t *c = (jlos_x86_regs_t *)child_cpustate;
    c->eax = 0;
    uint32_t p_base = (uint32_t)parent_stack;
    uint32_t c_base = (uint32_t)child_stack;
    uint32_t p_top  = p_base + parent_stack_size;
    uint32_t c_top  = c_base + child_stack_size;
    uint32_t iret_sp;
    bool     use_chain = false;
    bool     use_retpc = false;
    int32_t  K = 0;

    if (fork_ebp_ref >= p_base + 8u && fork_ebp_ref < p_top) {
        uint32_t *ebp0_ptr = (uint32_t *)fork_ebp_ref;
        uint32_t  ebp1    = ebp0_ptr[0];
        uint32_t  retpc0  = ebp0_ptr[1];
        /* 帧链法: [ebp]=上一帧 ebp, [ebp+4]=外层 retpc. 开帧指针编译命中. */
        if (retpc0 == fork_return_pc &&
            ebp1 >= p_base + 8u && ebp1 < p_top) {
            K = (int32_t)(ebp1 + 16u) - (int32_t)fork_esp_ref;
            if (K >= 16 && K <= 256) {
                use_chain = true;
            }
        }
    }

    if (!use_chain) {
        /* K_DEAD=-24: 6 args×4B + call retaddr 4B − addl$24 清栈 4B = -24,
         * 0 方差硬编码. 边界检查按 K 符号单向给 margin (pt_regs 余量 16B). */
        const int32_t K_DEAD = -24;
        uint32_t need_below = (K_DEAD < 0) ? ((uint32_t)(-K_DEAD) + 16u) : 0u;
        uint32_t need_above = (K_DEAD > 0) ? ((uint32_t)( K_DEAD) + 16u) : 0u;
        bool dead_ok = true;
        if ((need_below && fork_esp_ref < p_base + need_below) ||
            (need_above && fork_esp_ref + need_above > p_top)) {
            dead_ok = false;
        }
        if (dead_ok) {
            K = K_DEAD;
            use_retpc = true;
        }

        if (!use_retpc) {
            /* SAFE 兜底: 仅当 esp_ref 越界 (极端/测试异常) 时启用 */
            const int32_t K_FB = -24;
            uint32_t fb_nb = (K_FB < 0) ? ((uint32_t)(-K_FB) + 16u) : 0u;
            uint32_t fb_na = (K_FB > 0) ? ((uint32_t)( K_FB) + 16u) : 0u;
            if ((fb_nb && fork_esp_ref < p_base + fb_nb) ||
                (fb_na && fork_esp_ref + fb_na > p_top)) {
                iret_sp = (child_stack_size >= 128u)? (c_top - 128u) : (c_base + 32u);
                K = 0;
                goto DONE_SET_IRET;
            }
            K = K_FB;
        }
    }

    /* K 转父栈 esp_business, 再映射到子栈 iret_sp; 越界自动 clamp SAFE 兜底 */
    uint32_t esp_business;
    if (K < 0) {
        uint32_t mag = (uint32_t)(-K);
        if (mag >= fork_esp_ref - p_base) {
            iret_sp = (child_stack_size >= 128u)? (c_top - 128u) : (c_base + 32u);
            goto DONE_SET_IRET;
        }
        esp_business = fork_esp_ref - mag;
    } else {
        uint32_t mag = (uint32_t)K;
        if (fork_esp_ref + mag >= p_top) {
            iret_sp = (child_stack_size >= 128u)? (c_top - 128u) : (c_base + 32u);
            goto DONE_SET_IRET;
        }
        esp_business = fork_esp_ref + mag;
    }
    if (esp_business < p_base + 4u || esp_business >= p_top) {
        iret_sp = (child_stack_size >= 128u)? (c_top - 128u) : (c_base + 32u);
        goto DONE_SET_IRET;
    }
    iret_sp = esp_business - p_base + c_base;

DONE_SET_IRET:
    if (!is_user_process) {
        c->user_esp = iret_sp;
    }

    {
        const uint32_t OFF_EIP = 36u;
        uint8_t *dst56 = (uint8_t *)iret_sp - OFF_EIP;
        bool     memcpy_ok = (dst56 >= (uint8_t *)c_base &&
                              dst56 + sizeof(jlos_x86_regs_t) <= (uint8_t *)c_top);
        if (!memcpy_ok) {
            printk_err("fork prepare child: memcpy oob dst56=%x sz=%u\n",
                   (unsigned)dst56, (unsigned)sizeof(jlos_x86_regs_t));
            jlos_hal_halt();
        }
        jlos_x86_regs_t *onstack = (jlos_x86_regs_t *)dst56;
        __builtin_memcpy(onstack, child_cpustate, sizeof(jlos_x86_regs_t));

        if (!is_user_process) {
            /* 0x10246 = 0x10046 | IF=1 (bit9): child 不经 entry_stub 的 sti */
            c->eip      = fork_resume_pc;
            c->cs       = 0x10u;
            c->eflags   = 0x10246u;
            c->user_esp = iret_sp;
            c->user_ss  = 0u;
        }
        onstack->eip      = c->eip;
        onstack->cs       = c->cs;
        onstack->eflags   = c->eflags;
        onstack->user_esp = c->user_esp;
        onstack->user_ss  = c->user_ss;
    }

    (void)parent_task;
    (void)child_task;
    (void)parent_cpustate;
}

