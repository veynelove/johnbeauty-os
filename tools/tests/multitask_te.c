#include <tools/tests/multitask_te.h>
#include <hal/user_syscall.h>

extern void printf(const char *);

jlos_task_t task1;
jlos_task_t task2;
jlos_task_t task_user;

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

void task_user_fn()
{
    uint32_t pid = jlos_user_get_pid();
    jlos_user_printf("Hello from ring3! PID=%u\n", pid);
    jlos_user_get_tasks_info();
    jlos_user_sleep(100);
    jlos_user_puts("Wake up!\n");
    jlos_user_exit(0);
}

void multitask_test(jlos_mmu_t *mmu, jlos_task_manager_t *task_manager_)
{
    printf("multitask_test: adding 2 tasks (task_A, task_B)\n");
    if (jlos_task_init(&task1, mmu, task_a, "task_a") < 0) {
        printf("multitask_test: task_a init failed\n");
        return;
    }
    if (jlos_task_init(&task2, mmu, task_b, "task_b") < 0) {
        printf("multitask_test: task_b init failed\n");
        return;
    }
    jlos_task_manager_add_task(task_manager_, &task1);
    jlos_task_manager_add_task(task_manager_, &task2);

    if (jlos_task_init_user(&task_user, mmu, task_user_fn, "ring3") < 0) {
        printf("multitask_test: ring3 init failed\n");
        return;
    }
    jlos_task_manager_add_task(task_manager_, &task_user);
}
