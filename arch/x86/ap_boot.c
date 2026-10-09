/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/ap_boot.h>
#include <arch/x86/trampoline.h>
#include <arch/x86/lapic.h>
#include <arch/x86/smp.h>
#include <arch/x86/gdt.h>
#include <arch/x86/interrupts.h>
#include <hal/smp.h>
#include <hal/paging.h>
#include <hal/context.h>
#include <hal/clock_event.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/initcall.h>
#include <kernel/multitask.h>
#include <kernel/hrtimer.h>
#include <kernel/timek.h>

#define JLOS_KERNEL_LOG_SUBSYS "smp"
#include <kernel/printk.h>

static uint32_t s_ap_cpu_id;

static void jlos_ap_tsc_delay(uint32_t cycles)
{
    uint32_t start, now;
    __asm__ __volatile__("rdtsc" : "=a"(start) : : "edx");
    do {
        __asm__ __volatile__("rdtsc" : "=a"(now) : : "edx");
    } while ((now - start) < cycles);
}

static void jlos_ap_setup_trampoline(uint32_t ap_stack_top)
{
    uint8_t *dst = (uint8_t *)PHYS_TO_VIRT(JLOS_X86_AP_TRAMPOLINE_PHYS);
    uint32_t size = (uint32_t)(jlos_x86_trampoline_end - jlos_x86_trampoline_start);
    jlos_memcpy(dst, jlos_x86_trampoline_start, size);

    /* GDT 描述符 base: trampoline 内 GDT 的物理地址 */
    uint32_t gdt_phys = JLOS_X86_AP_TRAMPOLINE_PHYS +
        (uint32_t)(jlos_x86_trampoline_gdt - jlos_x86_trampoline_start);
    *(uint32_t *)(dst + (uint32_t)(jlos_x86_trampoline_gdt_desc -
        jlos_x86_trampoline_start) + 2) = gdt_phys;

    /* far_ptr offset: prot32 的物理地址 (16 位实模式远跳) */
    uint32_t prot32_phys = JLOS_X86_AP_TRAMPOLINE_PHYS +
        (uint32_t)(jlos_x86_trampoline_prot32 - jlos_x86_trampoline_start);
    *(uint16_t *)(dst + (uint32_t)(jlos_x86_trampoline_far_ptr -
        jlos_x86_trampoline_start)) = (uint16_t)prot32_phys;

    /* high_ptr offset: jlos_ap_main 虚拟地址 (32 位高半核远跳) */
    *(uint32_t *)(dst + (uint32_t)(jlos_x86_trampoline_high_ptr -
        jlos_x86_trampoline_start)) = (uint32_t)jlos_ap_main;

    /* CR4: BSP 的 CR4 (PSE|OSFXSR|PGE 等), AP 必须在开 PG 前设好 */
    uint32_t cr4;
    __asm__ __volatile__("movl %%cr4, %0" : "=r"(cr4));
    *(uint32_t *)(dst + (uint32_t)(jlos_x86_trampoline_cr4 -
        jlos_x86_trampoline_start)) = cr4;

    /* CR3: 内核页目录物理地址 */
    *(uint32_t *)(dst + (uint32_t)(jlos_x86_trampoline_cr3 -
        jlos_x86_trampoline_start)) =
        VIRT_TO_PHYS((uint32_t)s_kernel_paging_context.root);

    /* AP 内核栈顶 */
    *(uint32_t *)(dst + (uint32_t)(jlos_x86_trampoline_stack -
        jlos_x86_trampoline_start)) = ap_stack_top;
}

static void jlos_ap_send_init_sipi(uint32_t apic_id)
{
    jlos_arch_lapic_send_ipi(apic_id, JLOS_ARCH_APIC_ICR_INIT);
    jlos_ap_tsc_delay(JLOS_AP_INIT_DELAY_CYCLES);

    uint32_t vector = JLOS_X86_AP_TRAMPOLINE_VECTOR;
    jlos_arch_lapic_send_ipi(apic_id, JLOS_ARCH_APIC_ICR_STARTUP | vector);
    jlos_ap_tsc_delay(JLOS_AP_SIPI_DELAY_CYCLES);
    jlos_arch_lapic_send_ipi(apic_id, JLOS_ARCH_APIC_ICR_STARTUP | vector);
    jlos_ap_tsc_delay(JLOS_AP_SIPI_DELAY_CYCLES);
}

void jlos_ap_main(void)
{
    uint32_t cpu = s_ap_cpu_id;

    jlos_arch_cpu_gdt_init(cpu);
    jlos_arch_irq_load_idt();
    jlos_arch_lapic_init();
    jlos_arch_tss_init();

    this_cpu_write(jlos_cpu_online, true);
    printk_info("AP %u online\n", cpu);

    __asm__ __volatile__("sti");

    jlos_clock_event_select();
    jlos_hrtimer_base_init_cpu();
    jlos_tick_init_cpu();
    
    jlos_sched_init_cpu();
    printk_info("ap %u idle ready\n", cpu);

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

void jlos_hal_smp_boot_aps(void)
{
    uint32_t num_cpus = jlos_hal_num_cpus();
    if (num_cpus <= 1) {
        return;
    }

    jlos_smp_alloc_percpu_areas();

    /* Identity-map trampoline 页: AP 开 PG 后仍在此低地址执行, 直到 ljmp 高半核 */
    jlos_arch_map_entry(&s_kernel_paging_context, JLOS_X86_AP_TRAMPOLINE_PHYS,
        JLOS_X86_AP_TRAMPOLINE_PHYS, JLOS_PG_KERNEL_RW);
    jlos_hal_paging_flush_all_tlb();

    for (uint32_t cpu = 1; cpu < num_cpus; cpu++) {
        uint32_t apic_id = jlos_arch_smp_cpu_apic_id(cpu);

        void *stack_va = jlos_page_frame_malloc();
        if (!stack_va) {
            printk_err("AP %u: stack alloc failed\n", cpu);
            continue;
        }
        uint32_t stack_top = (uint32_t)stack_va + JLOS_PAGE_SIZE;

        per_cpu_write_cpu(jlos_cpu_id, cpu, cpu);
        per_cpu_write_cpu(jlos_cpu_kernel_stack, cpu, (void *)stack_top);
        per_cpu_write_cpu(jlos_cpu_kernel_stack_bottom, cpu, stack_va);
        per_cpu_write_cpu(jlos_cpu_kernel_stack_size, cpu, JLOS_PAGE_SIZE);
        per_cpu_write_cpu(jlos_cpu_online, cpu, false);

        s_ap_cpu_id = cpu;
        jlos_ap_setup_trampoline(stack_top);
        jlos_ap_send_init_sipi(apic_id);

        uint32_t timeout = JLOS_AP_ONLINE_TIMEOUT;
        while (!per_cpu_read_cpu(jlos_cpu_online, cpu) && timeout > 0) {
            __asm__ __volatile__("rep; nop" : : : "memory");
            timeout--;
        }

        if (per_cpu_read_cpu(jlos_cpu_online, cpu)) {
            printk_info("AP %u (apic %u) brought up\n", cpu, apic_id);
        } else {
            printk_err("AP %u (apic %u) failed to come online\n", cpu, apic_id);
        }
    }
}

JLOS_INITCALL(JLOS_INITCALL_LATE, jlos_hal_smp_boot_aps);
