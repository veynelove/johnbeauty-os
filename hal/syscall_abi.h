#ifndef __JLOS_HAL_SYSCALL_ABI_H
#define __JLOS_HAL_SYSCALL_ABI_H

#include <common/types.h>

#define JLOS_SYSCALL_MAX        256

enum jlos_syscall_num {
    JLOS_SYSCALL_WRITE              = 1,
    JLOS_SYSCALL_READ               = 2,
    JLOS_SYSCALL_EXIT               = 3,
    JLOS_SYSCALL_YIELD              = 4,
    JLOS_SYSCALL_GET_PID            = 5,
    JLOS_SYSCALL_SLEEP              = 6,
    JLOS_SYSCALL_GET_ERRNO          = 7,
    JLOS_SYSCALL_GET_TICKS          = 8,
    JLOS_SYSCALL_GET_TASKS_INFO     = 9,
    JLOS_SYSCALL_WAIT_PID           = 10,
    JLOS_SYSCALL_CREATE_PIPE        = 11,
    JLOS_SYSCALL_TASK_FD_CLOSE      = 12,
    JLOS_SYSCALL_TASK_BRK           = 13
};

enum jlos_syscall_misscode {
    SYSCALL_ENINVAL     = 1,
    SYSCALL_ENOSYS      = 2,
    SYSCALL_EFAULT      = 3,
    SYSCALL_EPERM       = 4,
    SYSCALL_ENOMEM      = 5,
};

#endif
