#include <tools/tests/multitask_te.h>

extern void sysprintf(char *);

jlos_task_t task1;
jlos_task_t task2;

void task_a()
{
    for (int i = 0; i < 10; i++) {
        sysprintf("task: A");
    }
}

void task_b()
{
    for (int i = 0; i < 10; i++) {
        sysprintf("task: B");
    }
}

void multitask_test(jlos_gdt_t *gdt, jlos_task_manager_t *task_manager_)
{
    jlos_task_init(&task1, gdt, task_a);
    jlos_task_init(&task2, gdt, task_b);
    jlos_task_manager_add_task(task_manager_, &task1);
    jlos_task_manager_add_task(task_manager_, &task2);
}
