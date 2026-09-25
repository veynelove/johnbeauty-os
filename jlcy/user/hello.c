#include <lib/syscall.h>

int main(int argc, char **argv)
{
    uint32_t pid = (uint32_t)get_pid();
    printf("hello from user ELF. PID = %u, argc = %d, argv[0] = %s\n", pid, argc, argv[0]);
    get_tasks_info();
    sleep(50);
    puts("user ELF: wakeup -> exit\n");
    return 7;
}
