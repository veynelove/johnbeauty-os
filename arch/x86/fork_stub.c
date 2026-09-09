
#include <kernel/multitask.h>
#include <hal/context.h>

void *jlos_arch_fork_invoke(void *mgr_v, void *parent_v)
{
    jlos_task_manager_t *mgr = (jlos_task_manager_t *)mgr_v;
    jlos_task_t *parent = (jlos_task_t *)parent_v;
    jlos_task_t *ret;

    __asm__ __volatile__ (
        "movl %%esp, %%ecx\n\t"
        "lea    1f, %%eax\n\t"
        "pushl %%eax\n\t"
        "pushl %%ecx\n\t"
        "pushl %2\n\t"
        "pushl %1\n\t"
        "call  *%3\n\t"
        "1:\n\t"
        "addl  $16, %%esp\n\t"
        : "=&a"(ret)
        : "r"(mgr), "r"(parent), "r"(jlos_process_fork)
        : "memory", "cc", "ecx");

    return (void *)ret;
}
