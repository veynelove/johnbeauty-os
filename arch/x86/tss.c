/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/tss.h>

void jlos_x86_tss_init(jlos_x86_tss_t *self, uintptr_t stack0, uint16_t ss0)
{
#ifdef CONFIG_X86_64
    (void)ss0;
    jlos_memset(self, 0, sizeof(jlos_x86_tss_t));
    self->rsp0 = stack0;
    self->io_map_base = sizeof(jlos_x86_tss_t);
#else
    jlos_memset(self, 0, sizeof(jlos_x86_tss_t));
    self->esp0 = (uint32_t)stack0;
    self->ss0 = ss0;
    self->io_map_base = offsetof(jlos_x86_tss_t, io_map_base);
#endif
}

void jlos_x86_tss_load(uint16_t selector)
{
    __asm__ __volatile__("ltr %0" : : "r"(selector));
}
