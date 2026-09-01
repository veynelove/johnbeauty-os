/* arch/x86/fork_stub.c - x86 fork_stub入口. 经典实现: ebx=label1(resume_pc), K_DEAD=-24(esp_ref→iret_sp偏移0方差). 跳板: mov$0,%eax; jmp*%ebx */
#include <kernel/multitask.h>
#include <hal/context.h>

/* HAL层使用void* opaque; 函数体首行转回真实类型(0运行时开销,类型安全) */
void *jlos_arch_fork_invoke(void *mgr_v, void *parent_v)
{
    jlos_task_manager_t *mgr    = (jlos_task_manager_t *)mgr_v;
    jlos_task_t         *parent = (jlos_task_t *)parent_v;
    uint32_t             caller_pc = (uint32_t)__builtin_return_address(0);
    jlos_task_t         *ret;

    /* 6实参: 6push(24B)+call push label1(4B)=28B; esp_ref=模板第1句硬件esp(6push前); child iret_sp=esp_ref-24(K_DEAD). */
    __asm__ __volatile__ (
        "movl %%esp, %%ecx\n\t"        /* ecx=fork_esp_ref (6push前真esp) */
        "movl %%ebp, %%edx\n\t"        /* edx=fork_ebp_ref (真 ebp) */
        "lea    1f, %%eax\n\t"         /* eax=label1地址=fork_resume_pc */
        "pushl %%eax\n\t"              /* arg6=resume_pc */
        "pushl %%edx\n\t"              /* arg5=ebp_ref */
        "pushl %%ecx\n\t"              /* arg4=esp_ref */
        "pushl %3\n\t"                 /* arg3=fork_return_pc */
        "pushl %2\n\t"                 /* arg2=parent */
        "pushl %1\n\t"                 /* arg1=mgr */
        "call  *%4\n\t"                /* jlos_process_fork(...); 父=eax=child*,子=iret→跳板→label1 */
        "1:\n\t"                       /* fork_resume_pc = ebx 子跳板目标 */
        "addl  $24, %%esp\n\t"         /* 清 6×4=24B 实参 */
        : "=&a"(ret)
        : "r"(mgr), "r"(parent), "r"(caller_pc), "r"(jlos_process_fork)
        : "memory", "cc", "ecx", "edx");

    return (void *)ret;
}
