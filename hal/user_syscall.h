#ifndef __JLOS_HAL_USER_SYSCALL_H
#define __JLOS_HAL_USER_SYSCALL_H

#include <common/types.h>
#include <hal/syscall_abi.h>

uint32_t jlos_user_syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static inline uint32_t jlos_user_write(const char *buf, uint32_t len)
{
    return jlos_user_syscall(JLOS_SYSCALL_WRITE, (uint32_t)buf, len, 0);
}

static inline void jlos_user_exit(int code)
{
    jlos_user_syscall(JLOS_SYSCALL_EXIT, (uint32_t)code, 0, 0);
}

static inline void jlos_user_yield(void)
{
    jlos_user_syscall(JLOS_SYSCALL_YIELD, 0, 0, 0);
}

#endif
