#ifndef _JLCY_SYSCALL_ABI_H
#define _JLCY_SYSCALL_ABI_H

#ifndef __ASSEMBLY__
#include <include/types.h>
#endif

#define SYSCALL_MAX 256

#define SYSCALL_WRITE              1
#define SYSCALL_READ               2
#define SYSCALL_EXIT               3
#define SYSCALL_YIELD              4
#define SYSCALL_GET_PID            5
#define SYSCALL_SLEEP              6
#define SYSCALL_GET_ERRNO          7
#define SYSCALL_GET_TICKS          8
#define SYSCALL_GET_TASKS_INFO     9
#define SYSCALL_WAIT_PID           10
#define SYSCALL_CREATE_PIPE        11
#define SYSCALL_TASK_FD_CLOSE      12
#define SYSCALL_TASK_BRK           13
#define SYSCALL_MMAP               14
#define SYSCALL_MUNMAP             15
#define SYSCALL_EXECVE             16
#define SYSCALL_OPEN               17
#define SYSCALL_LSEEK              18
#define SYSCALL_UNLINK             19
#define SYSCALL_FORK               20
#define SYSCALL_CLONE              21

#ifndef __ASSEMBLY__
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20

typedef struct mmap_arg_struct {
    uint32_t addr;
    uint32_t len;
    uint32_t prot;
    uint32_t flags;
    uint32_t fd;
    uint32_t offset;
} mmap_arg_struct_t;

enum syscall_misscode {
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

enum task_fd_std {
    TASK_FD_STD_IN    = 0,
    TASK_FD_STD_OUT   = 1,
    TASK_FD_STD_ERR   = 2
};
#endif

#endif
