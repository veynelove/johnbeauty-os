/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/msr.h>

void jlos_arch_msr_read(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
    uint32_t eax, edx;
    __asm__ __volatile__("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr));
    *lo = eax;
    *hi = edx;
}

void jlos_arch_msr_write(uint32_t msr, uint32_t lo, uint32_t hi)
{
    __asm__ __volatile__("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}
