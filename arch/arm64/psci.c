/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/psci.h>
#include <common/hardware/fdt.h>
#include <kernel/initcall.h>

#define JLOS_PSCI_CPU_ON_64     0xC4000003UL

#define JLOS_PSCI_CONDUIT_NONE  0
#define JLOS_PSCI_CONDUIT_HVC   1
#define JLOS_PSCI_CONDUIT_SMC   2

static int s_conduit = JLOS_PSCI_CONDUIT_NONE;

static void psci_parse_fdt(void)
{
    int32_t node = jlos_fdt_find_node("/psci");
    if (node < 0) {
        return;
    }
    uint32_t len = 0;
    const char *method = (const char *)jlos_fdt_get_prop(node, "method", &len);
    if (method && jlos_strcmp(method, "hvc") == 0) {
        s_conduit = JLOS_PSCI_CONDUIT_HVC;
    } else if (method && jlos_strcmp(method, "smc") == 0) {
        s_conduit = JLOS_PSCI_CONDUIT_SMC;
    }
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, psci_parse_fdt);

bool jlos_arch_psci_ready(void)
{
    return s_conduit != JLOS_PSCI_CONDUIT_NONE;
}

int32_t jlos_arch_psci_cpu_on(uint64_t mpidr, uintptr_t entry_phys, uintptr_t context)
{
    register uint64_t x0 __asm__("x0") = JLOS_PSCI_CPU_ON_64;
    register uint64_t x1 __asm__("x1") = mpidr;
    register uint64_t x2 __asm__("x2") = entry_phys;
    register uint64_t x3 __asm__("x3") = context;

    if (s_conduit == JLOS_PSCI_CONDUIT_HVC) {
        __asm__ __volatile__("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    } else if (s_conduit == JLOS_PSCI_CONDUIT_SMC) {
        __asm__ __volatile__("smc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    } else {
        return -1;
    }
    return ((int64_t)x0 == 0) ? 0 : -1;
}
