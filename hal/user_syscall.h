#ifndef __JLOS_HAL_USER_SYSCALL_H
#define __JLOS_HAL_USER_SYSCALL_H

#include <common/types.h>
#include <hal/syscall_abi.h>

int32_t jlos_user_syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static inline int32_t jlos_user_write(const char *buf, uint32_t len)
{
    return jlos_user_syscall(JLOS_SYSCALL_WRITE, (uint32_t)buf, len, 0);
}

static inline int32_t jlos_user_puts(const char *str)
{
    uint32_t len = 0;
    while (str[len]) len++;
    return jlos_user_write(str, len);
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

void jlos_user_printf(const char *fmt, ...);

#endif
