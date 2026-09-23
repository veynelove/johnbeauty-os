#ifndef _JLCY_SYSCALL_ABI_H
#define _JLCY_SYSCALL_ABI_H

#ifndef __ASSEMBLY__
#include <include/types.h>
#endif

#define JLOS_SYSCALL_MAX 256

#define JLOS_SYSCALL_WRITE              1
#define JLOS_SYSCALL_READ               2
#define JLOS_SYSCALL_EXIT               3
#define JLOS_SYSCALL_YIELD              4
#define JLOS_SYSCALL_GET_PID            5
#define JLOS_SYSCALL_SLEEP              6
#define JLOS_SYSCALL_GET_ERRNO          7
#define JLOS_SYSCALL_GET_TICKS          8
#define JLOS_SYSCALL_GET_TASKS_INFO     9
#define JLOS_SYSCALL_WAIT_PID           10
#define JLOS_SYSCALL_CREATE_PIPE        11
#define JLOS_SYSCALL_TASK_FD_CLOSE      12
#define JLOS_SYSCALL_TASK_BRK           13
#define JLOS_SYSCALL_MMAP               14
#define JLOS_SYSCALL_MUNMAP             15
#define JLOS_SYSCALL_EXECVE             16
#define JLOS_SYSCALL_OPEN               17
#define JLOS_SYSCALL_LSEEK              18
#define JLOS_SYSCALL_UNLINK             19

#ifndef __ASSEMBLY__
enum jlos_syscall_misscode {
    SYSCALL_ENINVAL  = 1,
    SYSCALL_ENOSYS   = 2,
    SYSCALL_EFAULT   = 3,
    SYSCALL_EPERM    = 4,
    SYSCALL_ENOMEM   = 5,
    SYSCALL_EPBIG    = 6,
    SYSCALL_ENOENT   = 7,
    SYSCALL_ENOSPC   = 8,
    SYSCALL_EIO      = 9,
};

enum jlos_task_fd_std {
    JLOS_TASK_FD_STD_IN    = 0,
    JLOS_TASK_FD_STD_OUT   = 1,
    JLOS_TASK_FD_STD_ERR   = 2
};
#endif

#endif
