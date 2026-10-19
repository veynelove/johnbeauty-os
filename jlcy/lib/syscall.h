/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLCY_LIB_SYSCALL_H
#define _JLCY_LIB_SYSCALL_H

#include <include/types.h>
#include <include/syscall_abi.h>

intptr_t syscall(uintptr_t num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);

static inline int32_t write(int32_t fd, const char *buf, uint32_t len)
{
    return (int32_t)syscall(SYSCALL_WRITE, (uintptr_t)fd, (uintptr_t)buf, len);
}

static inline int32_t read(int32_t fd, void *buf, uint32_t len)
{
    return (int32_t)syscall(SYSCALL_READ, (uintptr_t)fd, (uintptr_t)buf, len);
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
    return (int32_t)syscall(SYSCALL_EXIT, (uintptr_t)code, 0, 0);
}

static inline int32_t yield(void)
{
    return (int32_t)syscall(SYSCALL_YIELD, 0, 0, 0);
}

static inline int32_t get_pid(void)
{
    return (int32_t)syscall(SYSCALL_GET_PID, 0, 0, 0);
}

static inline int32_t sleep(uint32_t ticks)
{
    return (int32_t)syscall(SYSCALL_SLEEP, ticks, 0, 0);
}

static inline int32_t get_errno(void)
{
    return (int32_t)syscall(SYSCALL_GET_ERRNO, 0, 0, 0);
}

static inline uint32_t get_ticks(void)
{
    return (uint32_t)syscall(SYSCALL_GET_TICKS, 0, 0, 0);
}

static inline int32_t get_tasks_info(void)
{
    return (int32_t)syscall(SYSCALL_GET_TASKS_INFO, 0, 0, 0);
}

static inline int32_t wait_pid(uint32_t pid, int32_t *exit_code)
{
    return (int32_t)syscall(SYSCALL_WAIT_PID, pid, (uintptr_t)exit_code, 0);
}

static inline int32_t create_pipe(int32_t fds[2]) {
    return (int32_t)syscall(SYSCALL_CREATE_PIPE, (uintptr_t)fds, 0, 0);
}

static inline int32_t task_fd_close(int32_t fd)
{
    return (int32_t)syscall(SYSCALL_TASK_FD_CLOSE, (uintptr_t)fd, 0, 0);
}

static inline int32_t task_brk(uintptr_t addr)
{
    return (int32_t)syscall(SYSCALL_TASK_BRK, addr, 0, 0);
}

static inline void *mmap(void *addr, uint32_t len, uint32_t prot, uint32_t flags, int32_t fd, uint32_t offset)
{
    mmap_arg_struct_t a;
    a.addr = (uintptr_t)addr;
    a.len = len;
    a.prot = prot;
    a.flags = flags;
    a.fd = (uint32_t)fd;
    a.offset = offset;
    return (void *)syscall(SYSCALL_MMAP, (uintptr_t)&a, len, 0);
}

static inline int32_t munmap(void *addr, uint32_t len)
{
    return (int32_t)syscall(SYSCALL_MUNMAP, (uintptr_t)addr, len, 0);
}

static inline int32_t open(const char *path, uint32_t flags, uint32_t mode)
{
    return (int32_t)syscall(SYSCALL_OPEN, (uintptr_t)path, flags, mode);
}

static inline int32_t close(int32_t fd)
{
    return (int32_t)syscall(SYSCALL_TASK_FD_CLOSE, (uintptr_t)fd, 0, 0);
}

static inline int32_t lseek(int32_t fd, int32_t offset, uint32_t whence)
{
    return (int32_t)syscall(SYSCALL_LSEEK, (uintptr_t)fd, (uintptr_t)offset, whence);
}

static inline int32_t unlink(const char *path)
{
    return (int32_t)syscall(SYSCALL_UNLINK, (uintptr_t)path, 0, 0);
}

static inline int32_t fork(void)
{
    return (int32_t)syscall(SYSCALL_FORK, 0, 0, 0);
}

static inline int32_t clone(uint32_t flags, uintptr_t child_stack)
{
    return (int32_t)syscall(SYSCALL_CLONE, flags, child_stack, 0);
}

static inline int32_t signal(int32_t sig, void (*handler)(int))
{
    return (int32_t)syscall(SYSCALL_SIGNAL, (uintptr_t)sig, (uintptr_t)handler, 0);
}

static inline int32_t kill(uint32_t pid, int32_t sig)
{
    return (int32_t)syscall(SYSCALL_KILL, pid, (uintptr_t)sig, 0);
}

static inline int32_t gettimeofday(timeval_t *tv, void *tz)
{
    return (int32_t)syscall(SYSCALL_GETTIMEOFDAY, (uintptr_t)tv, (uintptr_t)tz, 0);
}

static inline int32_t clock_gettime(uint32_t clk_id, timespec_t *tp)
{
    return (int32_t)syscall(SYSCALL_CLOCK_GETTIME, clk_id, (uintptr_t)tp, 0);
}

static inline int32_t settimeofday(const timeval_t *tv, void *tz)
{
    return (int32_t)syscall(SYSCALL_SETTIMEOFDAY, (uintptr_t)tv, (uintptr_t)tz, 0);
}

static inline int32_t clock_settime(uint32_t clk_id, const timespec_t *tp)
{
    return (int32_t)syscall(SYSCALL_CLOCK_SETTIME, clk_id, (uintptr_t)tp, 0);
}

static inline int32_t nanosleep(const timespec_t *req)
{
    return (int32_t)syscall(SYSCALL_NANOSLEEP, (uintptr_t)req, 0, 0);
}

void printf(const char *fmt, ...);

#endif
