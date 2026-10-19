/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lib/syscall.h>

intptr_t syscall(uintptr_t num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
    intptr_t result;
    __asm__ __volatile__(
        "int $0x80\n\t"
        : "=a"(result)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return result;
}
