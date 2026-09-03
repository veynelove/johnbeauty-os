#ifndef _JLOS_HAL_USER_SYSCALL_H
#define _JLOS_HAL_USER_SYSCALL_H

#include <common/types.h>
#include <hal/syscall_abi.h>

int32_t jlos_user_syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static inline int32_t jlos_user_write(int32_t fd, const char *buf, uint32_t len)
{
    return jlos_user_syscall(JLOS_SYSCALL_WRITE, (uint32_t)fd, (uint32_t)buf, len);
}

static inline int32_t jlos_user_read(int32_t fd, void *buf, uint32_t len)
{
    return jlos_user_syscall(JLOS_SYSCALL_READ, (uint32_t)fd, (uint32_t)buf, len);
}

static inline int32_t jlos_user_puts(const char *str)
{
    /* 字符串字面量在 .rodata (0xC0xxxxxx 内核空间), access_ok 会拒绝。
     * 先拷贝到用户栈缓冲, 再传给 syscall */
    char buf[256];
    uint32_t len = 0;
    while (str[len] && len < sizeof(buf)) { buf[len] = str[len]; len++; }
    return jlos_user_write(JLOS_TASK_FD_STD_OUT, buf, len);
}

static inline int32_t jlos_user_exit(int code)
{
    return jlos_user_syscall(JLOS_SYSCALL_EXIT, (uint32_t)code, 0, 0);
}

static inline int32_t jlos_user_yield(void)
{
    return jlos_user_syscall(JLOS_SYSCALL_YIELD, 0, 0, 0);
}

static inline int32_t jlos_user_get_pid(void)
{
    return jlos_user_syscall(JLOS_SYSCALL_GET_PID, 0, 0, 0);
}

static inline int32_t jlos_user_sleep(uint32_t ticks)
{
    return jlos_user_syscall(JLOS_SYSCALL_SLEEP, ticks, 0, 0);
}

static inline int32_t jlos_user_get_errno(void)
{
    return jlos_user_syscall(JLOS_SYSCALL_GET_ERRNO, 0, 0, 0);
}

static inline uint32_t jlos_user_get_ticks(void)
{
    return (uint32_t)jlos_user_syscall(JLOS_SYSCALL_GET_TICKS, 0, 0, 0);
}

static inline int32_t jlos_user_get_tasks_info(void)
{
    return jlos_user_syscall(JLOS_SYSCALL_GET_TASKS_INFO, 0, 0, 0);
}

static inline int32_t jlos_user_wait_pid(uint32_t pid, int32_t *exit_code)
{
    return jlos_user_syscall(JLOS_SYSCALL_WAIT_PID, pid, (uint32_t)exit_code, 0);
}

static inline int32_t jlos_user_create_pipe(int32_t fds[2]) {
    return jlos_user_syscall(JLOS_SYSCALL_CREATE_PIPE, (uint32_t)fds, 0, 0);
}

static inline int32_t jlos_user_task_fd_close(int32_t fd)
{
    return jlos_user_syscall(JLOS_SYSCALL_TASK_FD_CLOSE, (uint32_t)fd, 0, 0);
}

static inline int32_t jlos_user_task_brk(uint32_t addr)
{
    return jlos_user_syscall(JLOS_SYSCALL_TASK_BRK, addr, 0, 0);
}
void jlos_user_printf(const char *fmt, ...);

#endif
