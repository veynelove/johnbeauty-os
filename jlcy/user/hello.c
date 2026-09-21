#include <lib/user_syscall.h>

int main(int argc, char **argv)
{
    uint32_t pid = (uint32_t)jlos_user_get_pid();
    printf("hello from user ELF. PID = %u, argc = %d, argv[0] = %s\n", pid, argc, argv[0]);
    jlos_user_get_tasks_info();
    jlos_user_sleep(50);
    jlos_user_puts("user ELF: wakeup -> exit\n");
    return 7;
}
