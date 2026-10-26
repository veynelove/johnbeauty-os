/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/interrupts.h>
#include <arch/arm64/gic.h>
#include <arch/arm64/smp.h>
#include <hal/irq.h>
#include <hal/hal_arch.h>
#include <hal/context.h>
#include <hal/cpu_state.h>
#include <hal/ext_state.h>
#include <hal/kernel_syscall.h>
#include <hal/paging.h>
#include <kernel/hrtimer.h>
#include <kernel/initcall.h>
#include <kernel/multitask.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "irq"
#include <kernel/printk.h>

static jlos_irq_manager_t s_interrupt_manager;
jlos_irq_manager_t *jlos_active_irq_manager = &s_interrupt_manager;

void jlos_irq_handler_init(jlos_irq_handler_t *self, jlos_irq_manager_t *mgr, uint32_t irq)
{
    self->interrupt_number = irq;
    self->interrupt_manager = mgr;
    self->handle_interrupt = jlos_irq_handler_handle_interrupt;
    mgr->handles[irq] = self;
    jlos_arch_gic_irq_unmask(irq);
}

uintptr_t jlos_irq_handler_handle_interrupt(jlos_irq_handler_t *self, uintptr_t frame)
{
    (void)self;
    return frame;
}

void jlos_irq_manager_init(void)
{
    jlos_irq_manager_t *self = &s_interrupt_manager;
    self->task_manager = g_task_manager_ptr;
}

void jlos_irq_manager_activate(void)
{
    __asm__ __volatile__("msr daifclr, #2" : : : "memory");
}

void jlos_irq_manager_deactivate(jlos_irq_manager_t *self)
{
    if (jlos_active_irq_manager == self) {
        jlos_active_irq_manager = nullptr;
        __asm__ __volatile__("msr daifset, #2" : : : "memory");
    }
}

uint16_t jlos_irq_manager_hw_offset(jlos_irq_manager_t *self)
{
    (void)self;
    return 0;
}

uintptr_t jlos_irq_manager_handle(uint32_t irq, uintptr_t frame)
{
    if (jlos_active_irq_manager != nullptr) {
        return jlos_irq_manager_do_handle(jlos_active_irq_manager, irq, frame);
    }
    return frame;
}

uintptr_t jlos_irq_manager_do_handle(jlos_irq_manager_t *self, uint32_t intid, uintptr_t frame)
{
    if (intid < 16) {
        if (intid == JLOS_ARCH_ARM64_IPI_TLB_SGI) {
            jlos_hal_paging_flush_tlb(*this_cpu_ptr(jlos_cpu_tlb_flush_va));
            *this_cpu_ptr(jlos_cpu_tlb_flush_done) = 1;
        } else if (intid == JLOS_ARCH_ARM64_IPI_RESCHEDULE_SGI) {
            if (self->task_manager && jlos_need_resched()) {
                jlos_task_manager_schedule(self->task_manager);
            }
        }
        return frame;
    }
    jlos_irq_handler_t *handler = (jlos_irq_handler_t *)self->handles[intid];
    if (handler && handler->handle_interrupt) {
        return handler->handle_interrupt(handler, frame);
    }
    printk_err("unhandled irq %u\n", intid);
    return frame;
}

void jlos_irq_manager_register(jlos_irq_manager_t *self, uint32_t irq, jlos_irq_handler_t *handler)
{
    self->handles[irq] = handler;
    jlos_arch_gic_irq_unmask(irq);
}

void jlos_irq_context_init(jlos_irq_context_t *context, uintptr_t arch_state_ptr)
{
    jlos_arm64_pt_regs_t *frame = (jlos_arm64_pt_regs_t *)arch_state_ptr;
    uint32_t ec = (uint32_t)(frame->esr >> 26);
    uint32_t dfsc = (uint32_t)(frame->esr & JLOS_ARM64_ESR_DFSC_MASK);
    bool from_lower = (ec == JLOS_ARM64_ESR_EC_INSTR_ABORT_LEL || ec == JLOS_ARM64_ESR_EC_DATA_ABORT_LEL);
    uint32_t error = from_lower ? 0x4 : 0;
    if (dfsc >= JLOS_ARM64_ESR_DFSC_ACCESS_FLAG_L1) {
        error |= 0x1;
    }
    if ((ec == JLOS_ARM64_ESR_EC_DATA_ABORT_LEL || ec == JLOS_ARM64_ESR_EC_DATA_ABORT_CEL) &&
        (frame->esr & JLOS_ARM64_ESR_WNR)) {
        error |= 0x2;
    }
    context->error = error;
    context->instruction_pointer = frame->pc;
    context->code_segment = 0;
    context->flags = (uint32_t)frame->pstate;
}

static void jlos_signal_deliver_check(jlos_arm64_pt_regs_t *frame)
{
    jlos_task_t *curr = jlos_hal_current_task();
    if (!curr || !curr->is_user_process) {
        return;
    }
    if (!jlos_cpu_state_is_user_mode((jlos_cpu_state_t *)frame)) {
        return;
    }
    jlos_signal_check_deliver(curr, (jlos_cpu_state_t *)frame);
}

void jlos_arm64_irq_dispatch(jlos_arm64_pt_regs_t *frame)
{
    jlos_irq_manager_t *mgr = jlos_active_irq_manager;
    if (!mgr) {
        printk_err("irq with no active manager\n");
        return;
    }
    uint32_t intid = jlos_arch_gic_irq_ack();
    if (intid >= JLOS_ARCH_GIC_INTID_SPURIOUS) {
        return;
    }
    jlos_irq_manager_do_handle(mgr, intid, (uintptr_t)frame);
    jlos_arch_gic_irq_eoi(intid);
    jlos_signal_deliver_check(frame);
}

void jlos_arm64_sync_dispatch(jlos_arm64_pt_regs_t *frame)
{
    uint32_t ec = (uint32_t)(frame->esr >> 26);
    if (ec == JLOS_ARM64_ESR_EC_SVC_A64) {
        jlos_hal_syscall_entry(nullptr, (uintptr_t)frame);
        return;
    }
    if (ec == JLOS_ARM64_ESR_EC_FP_TRAP) {
        jlos_arch_task_ext_trap_body();
        return;
    }
    if (ec == JLOS_ARM64_ESR_EC_INSTR_ABORT_LEL || ec == JLOS_ARM64_ESR_EC_INSTR_ABORT_CEL ||
        ec == JLOS_ARM64_ESR_EC_DATA_ABORT_LEL || ec == JLOS_ARM64_ESR_EC_DATA_ABORT_CEL) {

        jlos_irq_context_t context;
        jlos_irq_context_init(&context, (uintptr_t)frame);
        jlos_paging_page_fault_handler(&context);
        jlos_signal_deliver_check(frame);
        return;
    }
    printk_err("sync exception: ec = 0x%x, esr = 0x%lx, far = 0x%lx\n",
        ec, (unsigned long)frame->esr, (unsigned long)frame->far);
    printk_err("pc = 0x%lx, sp = 0x%lx, pstate = 0x%lx\n",
        (unsigned long)frame->pc, (unsigned long)frame->sp, (unsigned long)frame->pstate);

    for (;;) {
        jlos_hal_halt();
    }
}

void jlos_arm64_bad_dispatch(uint32_t kind)
{
    printk_err("unexpected exception kind = %u (fiq/serror/sp0)\n", kind);
    for (;;) {
        jlos_hal_halt();
    }
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_irq_manager_init);
JLOS_INITCALL(JLOS_INITCALL_POST, jlos_irq_manager_activate);
