/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <common/types.h>
#include <hal/ext_state.h>
#include <hal/smp.h>
#include <kernel/initcall.h>
#include <kernel/memory_manager.h>
#include <kernel/multitask.h>

#define JLOS_ARM64_CPACR_FPEN_SHIFT     20
#define JLOS_ARM64_CPACR_FPEN_MASK      (0x3UL << JLOS_ARM64_CPACR_FPEN_SHIFT)
#define JLOS_ARM64_CPACR_FPEN_TRAP_EL0  (0x1UL << JLOS_ARM64_CPACR_FPEN_SHIFT)
#define JLOS_ARM64_CPACR_FPEN_ALL       (0x3UL << JLOS_ARM64_CPACR_FPEN_SHIFT)

#define JLOS_ARM64_FPSR_OFFSET          512
#define JLOS_ARM64_FPCR_OFFSET          520

DEFINE_PER_CPU(jlos_task_t *, jlos_cpu_fpu_owner);

static void fpu_set_fpen(uint64_t fpen)
{
    uint64_t cpacr;
    __asm__ __volatile__("mrs %0, cpacr_el1" : "=r"(cpacr));
    cpacr = (cpacr & ~JLOS_ARM64_CPACR_FPEN_MASK) | fpen;
    __asm__ __volatile__("msr cpacr_el1, %0\n\tisb" : : "r"(cpacr) : "memory");
}

static void fpu_save_state(void *state)
{
    __asm__ __volatile__(
        "stp q0, q1,   [%0, #0]\n\t"
        "stp q2, q3,   [%0, #32]\n\t"
        "stp q4, q5,   [%0, #64]\n\t"
        "stp q6, q7,   [%0, #96]\n\t"
        "stp q8, q9,   [%0, #128]\n\t"
        "stp q10, q11, [%0, #160]\n\t"
        "stp q12, q13, [%0, #192]\n\t"
        "stp q14, q15, [%0, #224]\n\t"
        "stp q16, q17, [%0, #256]\n\t"
        "stp q18, q19, [%0, #288]\n\t"
        "stp q20, q21, [%0, #320]\n\t"
        "stp q22, q23, [%0, #352]\n\t"
        "stp q24, q25, [%0, #384]\n\t"
        "stp q26, q27, [%0, #416]\n\t"
        "stp q28, q29, [%0, #448]\n\t"
        "stp q30, q31, [%0, #480]\n\t"
        "mrs x9, fpsr\n\t"
        "str x9, [%0, #512]\n\t"
        "mrs x9, fpcr\n\t"
        "str x9, [%0, #520]\n\t"
        : : "r"(state) : "memory", "x9"
    );
}

static void fpu_restore_state(const void *state)
{
    __asm__ __volatile__(
        "ldp q0, q1,   [%0, #0]\n\t"
        "ldp q2, q3,   [%0, #32]\n\t"
        "ldp q4, q5,   [%0, #64]\n\t"
        "ldp q6, q7,   [%0, #96]\n\t"
        "ldp q8, q9,   [%0, #128]\n\t"
        "ldp q10, q11, [%0, #160]\n\t"
        "ldp q12, q13, [%0, #192]\n\t"
        "ldp q14, q15, [%0, #224]\n\t"
        "ldp q16, q17, [%0, #256]\n\t"
        "ldp q18, q19, [%0, #288]\n\t"
        "ldp q20, q21, [%0, #320]\n\t"
        "ldp q22, q23, [%0, #352]\n\t"
        "ldp q24, q25, [%0, #384]\n\t"
        "ldp q26, q27, [%0, #416]\n\t"
        "ldp q28, q29, [%0, #448]\n\t"
        "ldp q30, q31, [%0, #480]\n\t"
        "ldr x9, [%0, #512]\n\t"
        "msr fpsr, x9\n\t"
        "ldr x9, [%0, #520]\n\t"
        "msr fpcr, x9\n\t"
        : : "r"(state) : "memory", "x9"
    );
}

void jlos_arch_task_ext_init(jlos_task_t *task)
{
    jlos_memset(task->ext_state.raw, 0, JLOS_ARCH_EXT_STATE_SIZE);
    task->ext_state.used = false;
}

void jlos_arch_task_ext_destroy(jlos_task_t *task)
{
    jlos_task_t **owner = this_cpu_ptr(jlos_cpu_fpu_owner);
    if (*owner == task) {
        *owner = nullptr;
    }
}

void jlos_arch_task_ext_switch(void)
{
    fpu_set_fpen(JLOS_ARM64_CPACR_FPEN_TRAP_EL0);
}

void jlos_arch_task_ext_trap_body(void)
{
    jlos_task_t *curr = jlos_hal_current_task();
    if (!curr) {
        return;
    }
    jlos_task_t **owner = this_cpu_ptr(jlos_cpu_fpu_owner);
    fpu_set_fpen(JLOS_ARM64_CPACR_FPEN_ALL);
    if (*owner == curr) {
        return;
    }
    if (*owner) {
        fpu_save_state((*owner)->ext_state.raw);
    }
    fpu_restore_state(curr->ext_state.raw);
    *owner = curr;
    curr->ext_state.used = true;
}

static void jlos_arm64_fpu_boot_init(void)
{
    fpu_set_fpen(JLOS_ARM64_CPACR_FPEN_ALL);
}

JLOS_INITCALL(JLOS_INITCALL_CORE, jlos_arm64_fpu_boot_init);
