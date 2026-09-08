/* arch/x86/fork_stub.c - x86 fork_stub 入口. 子进程首次运行经 fork_entry_stub 切 esp 后 jmp 到 label1(resume_pc). */
#include <kernel/multitask.h>
#include <hal/context.h>

/* HAL层使用void* opaque; 函数体首行转回真实类型(0运行时开销,类型安全) */
void *jlos_arch_fork_invoke(void *mgr_v, void *parent_v)
{
    jlos_task_manager_t *mgr    = (jlos_task_manager_t *)mgr_v;
    jlos_task_t         *parent = (jlos_task_t *)parent_v;
    jlos_task_t         *ret;

    /* 4 实参: 4push(16B); esp_ref=第1句硬件 esp(4push 前); 子 label1 esp = esp_ref - 16. */
    __asm__ __volatile__ (
        "movl %%esp, %%ecx\n\t"        /* ecx=fork_esp_ref (4push 前真 esp) */
        "lea    1f, %%eax\n\t"         /* eax=label1 地址=fork_resume_pc */
        "pushl %%eax\n\t"              /* arg4=resume_pc */
        "pushl %%ecx\n\t"              /* arg3=esp_ref */
        "pushl %2\n\t"                 /* arg2=parent */
        "pushl %1\n\t"                 /* arg1=mgr */
        "call  *%3\n\t"                /* jlos_process_fork(...); 父=eax=child* */
        "1:\n\t"                       /* fork_resume_pc = ebx 子跳板目标 */
        "addl  $16, %%esp\n\t"         /* 清 4×4=16B 实参 */
        : "=&a"(ret)
        : "r"(mgr), "r"(parent), "r"(jlos_process_fork)
        : "memory", "cc", "ecx");

    return (void *)ret;
}
