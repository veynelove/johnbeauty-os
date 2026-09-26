#ifndef _JLOS_HAL_SYSCALL_ABI_H
#define _JLOS_HAL_SYSCALL_ABI_H

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
    JLOS_SYSCALL_TASK_BRK           = 13,
    JLOS_SYSCALL_MMAP               = 14,
    JLOS_SYSCALL_MUNMAP             = 15,
    JLOS_SYSCALL_EXECVE             = 16,
    JLOS_SYSCALL_OPEN               = 17,
    JLOS_SYSCALL_LSEEK              = 18,
    JLOS_SYSCALL_UNLINK             = 19,
    JLOS_SYSCALL_FORK               = 20,
    JLOS_SYSCALL_CLONE              = 21,
    JLOS_SYSCALL_SIGNAL             = 22,
    JLOS_SYSCALL_KILL               = 23,
    JLOS_SYSCALL_SIGRETURN          = 24,
};

enum jlos_syscall_misscode {
    SYSCALL_ENINVAL                 = 1,
    SYSCALL_ENOSYS                  = 2,
    SYSCALL_EFAULT                  = 3,
    SYSCALL_EPERM                   = 4,
    SYSCALL_ENOMEM                  = 5,
    SYSCALL_EPBIG                   = 6,
    SYSCALL_ENOENT                  = 7,
    SYSCALL_ENOSPC                  = 8,
    SYSCALL_EIO                     = 9,
};

enum jlos_task_fd_std {
    JLOS_TASK_FD_STD_IN             = 0,
    JLOS_TASK_FD_STD_OUT            = 1,
    JLOS_TASK_FD_STD_ERR            = 2
};

#define JLOS_PROT_READ      0x1
#define JLOS_PROT_WRITE     0x2
#define JLOS_PROT_EXEC      0x4

#define JLOS_MMAP_SHARED    0x01
#define JLOS_MMAP_PRIVATE   0x02
#define JLOS_MMAP_FIXED     0x10
#define JLOS_MMAP_ANON      0x20

#define JLOS_SIGNAL_NUM     32

#define JLOS_SIGHUP         1
#define JLOS_SIGINT         2
#define JLOS_SIGKILL        9
#define JLOS_SIGUSR1        10
#define JLOS_SIGSEGV        11
#define JLOS_SIGUSR2        12
#define JLOS_SIGTERM        15

#endif
