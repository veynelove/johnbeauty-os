#ifndef _JLCY_LIB_SYSCALL_H
#define _JLCY_LIB_SYSCALL_H

#include <include/types.h>
#include <include/syscall_abi.h>

int32_t syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static inline int32_t write(int32_t fd, const char *buf, uint32_t len)
{
    return syscall(SYSCALL_WRITE, (uint32_t)fd, (uint32_t)buf, len);
}

static inline int32_t read(int32_t fd, void *buf, uint32_t len)
{
    return syscall(SYSCALL_READ, (uint32_t)fd, (uint32_t)buf, len);
}

static inline int32_t puts(const char *str)
{
    char buf[256];
    uint32_t len = 0;
    while (str[len] && len < sizeof(buf)) { buf[len] = str[len]; len++; }
    return write(TASK_FD_STD_OUT, buf, len);
}

static inline int32_t exit(int code)
{
    return syscall(SYSCALL_EXIT, (uint32_t)code, 0, 0);
}

static inline int32_t yield(void)
{
    return syscall(SYSCALL_YIELD, 0, 0, 0);
}

static inline int32_t get_pid(void)
{
    return syscall(SYSCALL_GET_PID, 0, 0, 0);
}

static inline int32_t sleep(uint32_t ticks)
{
    return syscall(SYSCALL_SLEEP, ticks, 0, 0);
}

static inline int32_t get_errno(void)
{
    return syscall(SYSCALL_GET_ERRNO, 0, 0, 0);
}

static inline uint32_t get_ticks(void)
{
    return (uint32_t)syscall(SYSCALL_GET_TICKS, 0, 0, 0);
}

static inline int32_t get_tasks_info(void)
{
    return syscall(SYSCALL_GET_TASKS_INFO, 0, 0, 0);
}

static inline int32_t wait_pid(uint32_t pid, int32_t *exit_code)
{
    return syscall(SYSCALL_WAIT_PID, pid, (uint32_t)exit_code, 0);
}

static inline int32_t create_pipe(int32_t fds[2]) {
    return syscall(SYSCALL_CREATE_PIPE, (uint32_t)fds, 0, 0);
}

static inline int32_t task_fd_close(int32_t fd)
{
    return syscall(SYSCALL_TASK_FD_CLOSE, (uint32_t)fd, 0, 0);
}

static inline int32_t task_brk(uint32_t addr)
{
    return syscall(SYSCALL_TASK_BRK, addr, 0, 0);
}

static inline void *mmap(void *addr, uint32_t len, uint32_t prot, uint32_t flags, int32_t fd, uint32_t offset)
{
    mmap_arg_struct_t a;
    a.addr = (uint32_t)addr;
    a.len = len;
    a.prot = prot;
    a.flags = flags;
    a.fd = (uint32_t)fd;
    a.offset = offset;
    return (void *)(uint32_t)syscall(SYSCALL_MMAP, (uint32_t)&a, len, 0);
}

static inline int32_t munmap(void *addr, uint32_t len)
{
    return syscall(SYSCALL_MUNMAP, (uint32_t)addr, len, 0);
}

static inline int32_t open(const char *path, uint32_t flags, uint32_t mode)
{
    return syscall(SYSCALL_OPEN, (uint32_t)path, flags, mode);
}

static inline int32_t close(int32_t fd)
{
    return syscall(SYSCALL_TASK_FD_CLOSE, (uint32_t)fd, 0, 0);
}

static inline int32_t lseek(int32_t fd, int32_t offset, uint32_t whence)
{
    return syscall(SYSCALL_LSEEK, (uint32_t)fd, (uint32_t)offset, whence);
}

static inline int32_t unlink(const char *path)
{
    return syscall(SYSCALL_UNLINK, (uint32_t)path, 0, 0);
}

static inline int32_t fork(void)
{
    return syscall(SYSCALL_FORK, 0, 0, 0);
}

static inline int32_t clone(uint32_t flags, uint32_t child_stack)
{
    return syscall(SYSCALL_CLONE, flags, child_stack, 0);
}

void printf(const char *fmt, ...);

#endif
