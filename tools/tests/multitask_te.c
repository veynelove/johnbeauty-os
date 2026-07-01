#include <tools/tests/multitask_te.h>

extern void printf(const char *);

jlos_task_t task1;
jlos_task_t task2;

void task_a()
{
    for (int i = 0; i < 10; i++) {
        printf("task: A\n");
    }
}

void task_b()
{
    for (int i = 0; i < 10; i++) {
        printf("task: B\n");
    }
}

void multitask_test(jlos_mmu_t *mmu, jlos_task_manager_t *task_manager_)
{
    printf("multitask_test: adding 2 tasks (task_A, task_B)\n");
    jlos_task_init(&task1, mmu, task_a);
    jlos_task_init(&task2, mmu, task_b);
    jlos_task_manager_add_task(task_manager_, &task1);
    jlos_task_manager_add_task(task_manager_, &task2);
}
