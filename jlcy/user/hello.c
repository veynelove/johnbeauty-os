/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lib/syscall.h>

int main(int argc, char **argv)
{
    timespec_t ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint32_t pid = (uint32_t)get_pid();
    printf("hello from user ELF. PID = %u, argc = %d, argv[0] = %s, curr time: %d\n", pid, argc, argv[0], ts.tv_sec);
    get_tasks_info();
    sleep(50);
    puts("user ELF: wakeup -> exit\n");
    return 7;
}
