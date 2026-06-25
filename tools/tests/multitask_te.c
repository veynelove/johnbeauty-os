#include <tools/tests/multitask_te.h>

extern void sysprintf(char *);

void task_a()
{
    while(1) {
        sysprintf("A");
    }
}

void task_b()
{
    while(1) {
        sysprintf("B");
    }
}

void multitask_test(jlos_gdt_t *gdt, jlos_task_manager_t *task_manager_)
{
    jlos_task_t task1;
    jlos_task_init(&task1, gdt, task_a);
    jlos_task_t task2;
    jlos_task_init(&task2, gdt, task_b);
    jlos_task_manager_add_task(task_manager_, &task1);
    jlos_task_manager_add_task(task_manager_, &task2);
}
