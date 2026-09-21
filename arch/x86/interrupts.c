#include <arch/x86/interrupts.h>
#include <arch/x86/cpu_state.h>
#include <arch/x86/io.h>
#include <arch/x86/gdt.h>
#include <hal/timer.h>
#include <hal/paging.h>
#include <hal/irq.h>
#include <hal/ext_state.h>
#include <hal/hal.h>
#include <hal/mmu.h>
#include <hal/hal_arch.h>
#include <kernel/initcall.h>
#include <kernel/paging.h>

#define JLOS_KERNEL_LOG_SUBSYS "irq"
#include <kernel/printk.h>

extern void jlos_arch_tss_init_for_asm(void);
extern jlos_task_t *g_current_task_ptr;

static jlos_irq_manager_t s_interrupt_manager;
jlos_irq_manager_t        *jlos_active_irq_manager = &s_interrupt_manager;

/* 保存 syscall 的 ring3 上下文，用于从 ring0 返回 ring3 */
uint32_t                        jlos_syscall_ring3_ctx = 0;
uint32_t                        jlos_syscall_ring3_kstack = 0;

jlos_gate_descriptor_t          jlos_interrupt_descriptor_table[256];

/* PIC IRQ 动态屏蔽：驱动注册 handler 时自动 unmask，避免无处理的 IRQ 导致 UNHANDLED */
static uint8_t                  s_pic_master_mask = 0xFA;  /* 1111 1010 — 默认开 IRQ0(PIT) 和 IRQ2(cascade) */
static uint8_t                  s_pic_slave_mask  = 0xFF;  /* 1111 1111 — 默认关 slave 全部（IRQ8~15） */

typedef struct {
    uint16_t size;
    uint32_t base;
} __attribute__((packed)) jlos_idt_pointer_t;

static void jlos_set_interrupt_descriptor_table_entry(uint8_t interrupt_number,
    uint16_t code_segment_selector_offset, void (*handler)(), uint8_t descriptor_privilege_level,
    uint8_t descriptor_type)
{
    const uint8_t IDT_DESC_PRESENT = 0x80;
    
    jlos_interrupt_descriptor_table[interrupt_number].handle_address_low_bits =
        ((uint32_t)handler) & 0xFFFF;
    jlos_interrupt_descriptor_table[interrupt_number].handle_address_high_bits =
        (((uint32_t)handler) >> 16) & 0xFFFF;
    jlos_interrupt_descriptor_table[interrupt_number].gdt_codeSegmentSelector =
        code_segment_selector_offset;
    jlos_interrupt_descriptor_table[interrupt_number].access = (IDT_DESC_PRESENT | descriptor_type
        | ((descriptor_privilege_level & 3) << 5));
    jlos_interrupt_descriptor_table[interrupt_number].reserved = 0;
}

static void jlos_irq_pic_unmask(jlos_irq_manager_t* self, uint8_t vector)
{
    uint16_t base = self->hardware_interrupt_offset;
    if (vector < base || vector >= base + 16) return; /* 非 PIC IRQ 范围，不管 */
    uint8_t irq = vector - base;
    if (irq < 8) {
        s_pic_master_mask &= ~(1U << irq);
        jlos_port_io8_slow_write(&self->pic_master_data, s_pic_master_mask);
    } else {
        irq -= 8;
        s_pic_slave_mask &= ~(1U << irq);
        jlos_port_io8_slow_write(&self->pic_slave_data, s_pic_slave_mask);
        /* Slave 通过 IRQ2 接 master，确保 master IRQ2 不被屏蔽 */
        s_pic_master_mask &= ~(1U << 2);
        jlos_port_io8_slow_write(&self->pic_master_data, s_pic_master_mask);
    }
}

void jlos_irq_handler_init(jlos_irq_handler_t* self, jlos_irq_manager_t *interrupt_manager, uint8_t interrupt_number)
{
    self->interrupt_number = interrupt_number;
    self->interrupt_manager = interrupt_manager;
    self->handle_interrupt = jlos_irq_handler_handle_interrupt;
    interrupt_manager->handles[interrupt_number] = self;
    jlos_irq_pic_unmask(interrupt_manager, interrupt_number); /* 注册即 unmask */
}

uint32_t jlos_irq_handler_handle_interrupt(jlos_irq_handler_t* self, uint32_t esp)
{
    return esp;
    (void)self;
}

void jlos_irq_manager_init(void)
{
    jlos_irq_manager_t *self = &s_interrupt_manager;
    jlos_port_io8_slow_init(&self->pic_master_command, 0x20);
    jlos_port_io8_slow_init(&self->pic_master_data, 0x21);
    jlos_port_io8_slow_init(&self->pic_slave_command, 0xA0);
    jlos_port_io8_slow_init(&self->pic_slave_data, 0xA1);

    // ICW1: start initialization, edge triggered, cascade mode, ICW4 needed
    jlos_port_io8_slow_write(&self->pic_master_command, 0x11);
    jlos_port_io8_slow_write(&self->pic_slave_command, 0x11);

    // ICW2: remap IRQ base vectors
    jlos_port_io8_slow_write(&self->pic_master_data, (uint8_t)KERNEL_FIRST_INTERRUPT_VECTOR);
    jlos_port_io8_slow_write(&self->pic_slave_data, (uint8_t)(KERNEL_FIRST_INTERRUPT_VECTOR + 8));

    // ICW3: master has slave at IRQ2, slave cascade id 2
    jlos_port_io8_slow_write(&self->pic_master_data, 0x04);
    jlos_port_io8_slow_write(&self->pic_slave_data, 0x02);

    // ICW4: 8086 mode, no auto EOI, non-buffered, fully nested
    jlos_port_io8_slow_write(&self->pic_master_data, 0x01);
    jlos_port_io8_slow_write(&self->pic_slave_data, 0x01);

    // OCW1：动态策略——默认只开 IRQ0(PIT)+IRQ2(cascade)，驱动注册 handler 时自动 unmask
    s_pic_master_mask = 0xFA;
    s_pic_slave_mask  = 0xFF;
    jlos_port_io8_slow_write(&self->pic_master_data, s_pic_master_mask);
    jlos_port_io8_slow_write(&self->pic_slave_data, s_pic_slave_mask);

    self->task_manager = g_task_manager_ptr;
    self->hardware_interrupt_offset = KERNEL_FIRST_INTERRUPT_VECTOR;
    uint16_t code_segment = jlos_mmu_code_selector(jlos_mmu_get_kernel());
    const uint8_t IDT_INTERRUPT_GATE = 0xE;
    const uint8_t IDT_TRAP_GATE = 0xF;
    
    /* 初始化 TSS 基地址 (供汇编使用) */
    jlos_arch_tss_init_for_asm();
    
    for (uint16_t i = 0; i < 256; i++) {
        jlos_set_interrupt_descriptor_table_entry(i, code_segment, &jlos_irq_ignore_request, 0,
            IDT_INTERRUPT_GATE);
    }
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR, code_segment,
        &jlos_handle_interrupt_request0x00, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x01, code_segment,
        &jlos_handle_interrupt_request0x01, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x02, code_segment,
        &jlos_handle_interrupt_request0x02, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x03, code_segment,
        &jlos_handle_interrupt_request0x03, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x04, code_segment,
        &jlos_handle_interrupt_request0x04, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x05, code_segment,
        &jlos_handle_interrupt_request0x05, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x06, code_segment,
        &jlos_handle_interrupt_request0x06, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x07, code_segment,
        &jlos_handle_interrupt_request0x07, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x08, code_segment,
        &jlos_handle_interrupt_request0x08, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x09, code_segment,
        &jlos_handle_interrupt_request0x09, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x0A, code_segment,
        &jlos_handle_interrupt_request0x0a, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x0B, code_segment,
        &jlos_handle_interrupt_request0x0b, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x0C, code_segment,
        &jlos_handle_interrupt_request0x0c, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x0D, code_segment,
        &jlos_handle_interrupt_request0x0d, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x0E, code_segment,
        &jlos_handle_interrupt_request0x0e, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x0F, code_segment,
        &jlos_handle_interrupt_request0x0f, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(KERNEL_FIRST_INTERRUPT_VECTOR + 0x31, code_segment,
        &jlos_handle_interrupt_request0x31, 0, IDT_INTERRUPT_GATE);

    /* syscall 入口：DPL=3 允许 ring3 调用，但 handler 运行在 ring0（内核代码段） */
    jlos_set_interrupt_descriptor_table_entry(0x80, code_segment,
        &jlos_handle_interrupt_request0x80, 3, IDT_TRAP_GATE);

    jlos_set_interrupt_descriptor_table_entry(0x00, code_segment, &jlos_handle_exception0x00, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x01, code_segment, &jlos_handle_exception0x01, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x02, code_segment, &jlos_handle_exception0x02, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x03, code_segment, &jlos_handle_exception0x03, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x04, code_segment, &jlos_handle_exception0x04, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x05, code_segment, &jlos_handle_exception0x05, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x06, code_segment, &jlos_handle_exception0x06, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x07, code_segment, &jlos_handle_exception0x07, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x08, code_segment, &jlos_handle_exception0x08, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x09, code_segment, &jlos_handle_exception0x09, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x0A, code_segment, &jlos_handle_exception0x0a, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x0B, code_segment, &jlos_handle_exception0x0b, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x0C, code_segment, &jlos_handle_exception0x0c, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x0D, code_segment, &jlos_handle_exception0x0d, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x0E, code_segment, &jlos_handle_exception0x0e, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x0F, code_segment, &jlos_handle_exception0x0f, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x10, code_segment, &jlos_handle_exception0x10, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x11, code_segment, &jlos_handle_exception0x11, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x12, code_segment, &jlos_handle_exception0x12, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(0x13, code_segment, &jlos_handle_exception0x13, 0, IDT_INTERRUPT_GATE);

    jlos_port_io8_slow_write(&self->pic_master_command, 0x11);
    jlos_port_io8_slow_write(&self->pic_slave_command, 0x11);
    jlos_port_io8_slow_write(&self->pic_master_data, KERNEL_FIRST_INTERRUPT_VECTOR);
    jlos_port_io8_slow_write(&self->pic_slave_data, KERNEL_FIRST_INTERRUPT_VECTOR + 8);
    jlos_port_io8_slow_write(&self->pic_master_data, 0x04);
    jlos_port_io8_slow_write(&self->pic_slave_data, 0x02); 
    jlos_port_io8_slow_write(&self->pic_master_data, 0x01);
    jlos_port_io8_slow_write(&self->pic_slave_data, 0x01); 
    jlos_port_io8_slow_write(&self->pic_master_data, s_pic_master_mask);
    jlos_port_io8_slow_write(&self->pic_slave_data, s_pic_slave_mask); 

    jlos_idt_pointer_t idt;
    idt.size = 256 * sizeof(jlos_gate_descriptor_t) - 1;
    idt.base = (uint32_t)jlos_interrupt_descriptor_table;
    __asm__ __volatile__("lidt %0" : : "m" (idt));
}


void jlos_irq_manager_activate(void)
{
    __asm__("sti");
}

void jlos_irq_manager_deactivate(jlos_irq_manager_t* self)
{
    if (jlos_active_irq_manager == self) {
        jlos_active_irq_manager = NULL;
        __asm__("cli");
    }
}

uint32_t jlos_irq_manager_handle(uint8_t interrupt, uint32_t esp)
{
    if (jlos_active_irq_manager != NULL) {
        return jlos_irq_manager_do_handle(jlos_active_irq_manager, interrupt, esp);
    }
    return esp;
}

uint16_t jlos_irq_manager_hw_offset(jlos_irq_manager_t* self)
{
    return self->hardware_interrupt_offset;
}

uint32_t jlos_irq_manager_do_handle(jlos_irq_manager_t* self, uint8_t interrupt, uint32_t esp)
{
    /* page fault 必须在 IRQ 向量转换之前处理，否则 0x0E 被偏移成 0x2E */
    if (interrupt == 0x0E) {
        jlos_irq_context_t context;
        jlos_irq_context_init(&context, esp);
        jlos_paging_page_fault_handler(&context);
        return esp;
    }
    
    if (interrupt < 0x20 && interrupt != 0x07 && interrupt != 0x00 && interrupt != 0x02 &&
        self->handles[interrupt + self->hardware_interrupt_offset] == NULL) {
        jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)esp;
        uint32_t err = cpu->padding;   /* offset 32 = CPU 压入的真实 error code */
        printk_err("exception num=0x%x err=0x%x eip=0x%x cs=0x%x efl=0x%x uesp=0x%x uss=0x%x pid=%u\n",
               interrupt, err, cpu->eip, cpu->cs, cpu->eflags,
               cpu->user_esp, cpu->user_ss,
               g_current_task_ptr ? g_current_task_ptr->pid : 0);
        if (interrupt == 0x0D) {  /* #GP: error code 非零时为触犯的段选择子 */
            if (err) {
                printk_err("#GP sel=0x%x idx=%u TI=%u RPL=%u ext=%u\n",
                       err & 0xFFF8, (err >> 3) & 0x1FF,
                       (err >> 2) & 1, err & 3, (err >> 0) & 1 ? 0 : 1);
            } else {
                printk_err("#GP err=0\n");
            }
        }
        if (interrupt == 0x08) {  /* #DF 双重错误 → 即将 triple fault 关闭 CPU */
            printk_err("#DF double fault -> triple fault imminent\n");
        }
        /* #GP/#DF 为不可恢复致命错误, halt 以便读日志; 其余异常也 halt 避免雪崩 */
        for (;;) {
            jlos_hal_halt();
        }
    }

    if (interrupt == 0x07) {
        jlos_arch_task_ext_trap_body();
        return esp;
    }

    uint8_t vector = interrupt;
    if (interrupt < 16) {
        vector = interrupt + self->hardware_interrupt_offset;
    }

    if (self->handles[vector] != NULL) {
        jlos_irq_handler_t *handler = (jlos_irq_handler_t*)self->handles[vector];
        esp = handler->handle_interrupt(handler, esp);
    }
    else if (interrupt >= 16) {
        jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)esp;
        printk_err("unhandled interrupt 0x%x err=0x%x eip=0x%x cs=0x%x eflags=0x%x\n",
            interrupt, cpu->error, cpu->eip, cpu->cs, cpu->eflags);
    }

    /* IRQ0 (PIT): 先 tick 再调度，调度器读到最新 tick */
    if (interrupt == 0 && self && self->task_manager) {
        jlos_hal_timer_on_tick();
        jlos_task_t *curr = jlos_task_manager_curr_task_on_tick(self->task_manager);

        bool resched = self->task_manager->need_resched;
        if (!curr || curr->status != JLOS_TASK_RUNNING || (KERNEL_CONFIG_PREEMPTIVE && !curr->remain_slice)) {
            resched = true;
        }
        if (resched) {
            jlos_task_manager_schedule(self->task_manager);
        }
    }
    if (vector >= 0x20 && vector < 0x30) {
        jlos_port_io8_slow_write(&self->pic_master_command, 0x20);
        if (vector >= 0x28) {
            jlos_port_io8_slow_write(&self->pic_slave_command, 0x20);
        }
    }
    return esp;
}

void jlos_irq_manager_register(jlos_irq_manager_t* self, uint8_t interrupt, jlos_irq_handler_t* handler)
{
    self->handles[interrupt] = handler;
    jlos_irq_pic_unmask(self, interrupt);  /* 注册即 unmask */
}

void jlos_irq_context_init(jlos_irq_context_t *context, uint32_t arch_state_ptr)
{
    jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)arch_state_ptr;
    context->error = cpu->padding;
    context->instruction_pointer = cpu->eip;
    context->code_segment = cpu->cs;
    context->flags = cpu->eflags;
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, jlos_irq_manager_init);
JLOS_INITCALL(JLOS_INITCALL_POST, jlos_irq_manager_activate);
