/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lib/syscall.h>

intptr_t syscall(uintptr_t num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
    register uintptr_t x8 __asm__("x8") = num;
    register uintptr_t x0 __asm__("x0") = arg1;
    register uintptr_t x1 __asm__("x1") = arg2;
    register uintptr_t x2 __asm__("x2") = arg3;
    __asm__ __volatile__(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory"
    );
    return (intptr_t)x0;
}
