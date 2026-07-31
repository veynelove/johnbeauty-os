#include <arch/x86/interrupts.h>
#include <arch/x86/cpu_state.h>
#include <hal/timer.h>
#include <hal/paging.h>
#include <hal/irq.h>
#include <kernel/printk.h>

extern void jlos_arch_tss_init_for_asm(void);

jlos_interrupt_manager_t *jlos_active_interrupt_manager = NULL;

/* 保存 syscall 的 ring3 上下文，用于从 ring0 返回 ring3 */
uint32_t jlos_syscall_ring3_ctx = 0;
uint32_t jlos_syscall_ring3_kstack = 0;

typedef struct {
    uint16_t m_handle_address_low_bits;
    uint16_t m_gdt_codeSegmentSelector;
    uint8_t m_reserved;
    uint8_t m_access;
    uint16_t m_handle_address_high_bits;
} __attribute__((packed)) jlos_gate_descriptor_t;

jlos_gate_descriptor_t jlos_interrupt_descriptor_table[256];

typedef struct {
    uint16_t m_size;
    uint32_t m_base;
} __attribute__((packed)) jlos_idt_pointer_t;

static void jlos_set_interrupt_descriptor_table_entry(uint8_t m_interrupt_number,
    uint16_t code_segment_selector_offset, void (*handler)(), uint8_t descriptor_privilege_level,
    uint8_t descriptor_type)
{
    const uint8_t IDT_DESC_PRESENT = 0x80;
    
    jlos_interrupt_descriptor_table[m_interrupt_number].m_handle_address_low_bits =
        ((uint32_t)handler) & 0xFFFF;
    jlos_interrupt_descriptor_table[m_interrupt_number].m_handle_address_high_bits =
        (((uint32_t)handler) >> 16) & 0xFFFF;
    jlos_interrupt_descriptor_table[m_interrupt_number].m_gdt_codeSegmentSelector =
        code_segment_selector_offset;
    jlos_interrupt_descriptor_table[m_interrupt_number].m_access = (IDT_DESC_PRESENT | descriptor_type
        | ((descriptor_privilege_level & 3) << 5));
    jlos_interrupt_descriptor_table[m_interrupt_number].m_reserved = 0;
}

/* PIC IRQ 动态屏蔽：驱动注册 handler 时自动 unmask，避免无处理的 IRQ 导致 UNHANDLED */
static uint8_t s_pic_master_mask = 0xFA;  /* 1111 1010 — 默认开 IRQ0(PIT) 和 IRQ2(cascade) */
static uint8_t s_pic_slave_mask  = 0xFF;  /* 1111 1111 — 默认关 slave 全部（IRQ8~15） */

static void jlos_irq_pic_unmask(jlos_interrupt_manager_t* self, uint8_t vector)
{
    uint16_t base = self->m_hardware_interrupt_offset;
    if (vector < base || vector >= base + 16) return; /* 非 PIC IRQ 范围，不管 */
    uint8_t irq = vector - base;
    if (irq < 8) {
        s_pic_master_mask &= ~(1U << irq);
        jlos_port8_bit_slow_write(&self->m_pic_master_data, s_pic_master_mask);
    } else {
        irq -= 8;
        s_pic_slave_mask &= ~(1U << irq);
        jlos_port8_bit_slow_write(&self->m_pic_slave_data, s_pic_slave_mask);
        /* Slave 通过 IRQ2 接 master，确保 master IRQ2 不被屏蔽 */
        s_pic_master_mask &= ~(1U << 2);
        jlos_port8_bit_slow_write(&self->m_pic_master_data, s_pic_master_mask);
    }
}

void jlos_interrupt_handler_init(jlos_interrupt_handler_t* self, jlos_interrupt_manager_t *interrupt_manager, uint8_t interrupt_number)
{
    self->m_interrupt_number = interrupt_number;
    self->m_interrupt_manager = interrupt_manager;
    self->handle_interrupt = (jlos_interrupt_handler_func_t)jlos_interrupt_handler_handle_interrupt;
    interrupt_manager->handles[interrupt_number] = self;
    jlos_irq_pic_unmask(interrupt_manager, interrupt_number); /* 注册即 unmask */
}

void jlos_interrupt_handler_destroy(jlos_interrupt_handler_t* self)
{
    if (self->m_interrupt_manager->handles[self->m_interrupt_number] == self) {
        self->m_interrupt_manager->handles[self->m_interrupt_number] = NULL;
    }
}

uint32_t jlos_interrupt_handler_handle_interrupt(jlos_interrupt_handler_t* self, uint32_t m_esp)
{
    return m_esp;
}

void jlos_interrupt_manager_init(jlos_interrupt_manager_t* self, uint16_t hardware_interruptoffset,
    jlos_gdt_t* gdt, jlos_task_manager_t *task_manager)
{
    jlos_port8_bit_slow_init(&self->m_pic_master_command, 0x20);
    jlos_port8_bit_slow_init(&self->m_pic_master_data, 0x21);
    jlos_port8_bit_slow_init(&self->m_pic_slave_command, 0xA0);
    jlos_port8_bit_slow_init(&self->m_pic_slave_data, 0xA1);

    // ICW1: start initialization, edge triggered, cascade mode, ICW4 needed
    jlos_port8_bit_slow_write(&self->m_pic_master_command, 0x11);
    jlos_port8_bit_slow_write(&self->m_pic_slave_command, 0x11);

    // ICW2: remap IRQ base vectors
    jlos_port8_bit_slow_write(&self->m_pic_master_data, (uint8_t)hardware_interruptoffset);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, (uint8_t)(hardware_interruptoffset + 8));

    // ICW3: master has slave at IRQ2, slave cascade id 2
    jlos_port8_bit_slow_write(&self->m_pic_master_data, 0x04);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, 0x02);

    // ICW4: 8086 mode, no auto EOI, non-buffered, fully nested
    jlos_port8_bit_slow_write(&self->m_pic_master_data, 0x01);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, 0x01);

    // OCW1：动态策略——默认只开 IRQ0(PIT)+IRQ2(cascade)，驱动注册 handler 时自动 unmask
    s_pic_master_mask = 0xFA;
    s_pic_slave_mask  = 0xFF;
    jlos_port8_bit_slow_write(&self->m_pic_master_data, s_pic_master_mask);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, s_pic_slave_mask);

    self->task_manager = task_manager;
    self->m_hardware_interrupt_offset = hardware_interruptoffset;
    uint16_t code_segment = jlos_gdt_code_segment_selector(gdt);
    const uint8_t IDT_INTERRUPT_GATE = 0xE;
    const uint8_t IDT_TRAP_GATE = 0xF;
    
    /* 初始化 TSS 基地址 (供汇编使用) */
    jlos_arch_tss_init_for_asm();
    
    for (uint16_t i = 0; i < 256; i++) {
        self->handles[i] = NULL;
        jlos_set_interrupt_descriptor_table_entry(i, code_segment, &jlos_ignore_interrupt_request, 0,
            IDT_INTERRUPT_GATE);
    }
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset, code_segment,
        &jlos_handle_interrupt_request0x00, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x01, code_segment,
        &jlos_handle_interrupt_request0x01, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x02, code_segment,
        &jlos_handle_interrupt_request0x02, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x03, code_segment,
        &jlos_handle_interrupt_request0x03, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x04, code_segment,
        &jlos_handle_interrupt_request0x04, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x05, code_segment,
        &jlos_handle_interrupt_request0x05, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x06, code_segment,
        &jlos_handle_interrupt_request0x06, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x07, code_segment,
        &jlos_handle_interrupt_request0x07, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x08, code_segment,
        &jlos_handle_interrupt_request0x08, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x09, code_segment,
        &jlos_handle_interrupt_request0x09, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x0A, code_segment,
        &jlos_handle_interrupt_request0x0a, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x0B, code_segment,
        &jlos_handle_interrupt_request0x0b, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x0C, code_segment,
        &jlos_handle_interrupt_request0x0c, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x0D, code_segment,
        &jlos_handle_interrupt_request0x0d, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x0E, code_segment,
        &jlos_handle_interrupt_request0x0e, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x0F, code_segment,
        &jlos_handle_interrupt_request0x0f, 0, IDT_INTERRUPT_GATE);
    jlos_set_interrupt_descriptor_table_entry(hardware_interruptoffset + 0x31, code_segment,
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

    jlos_port8_bit_slow_write(&self->m_pic_master_command, 0x11);
    jlos_port8_bit_slow_write(&self->m_pic_slave_command, 0x11);
    jlos_port8_bit_slow_write(&self->m_pic_master_data, hardware_interruptoffset);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, hardware_interruptoffset + 8);
    jlos_port8_bit_slow_write(&self->m_pic_master_data, 0x04);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, 0x02); 
    jlos_port8_bit_slow_write(&self->m_pic_master_data, 0x01);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, 0x01); 
    jlos_port8_bit_slow_write(&self->m_pic_master_data, s_pic_master_mask);
    jlos_port8_bit_slow_write(&self->m_pic_slave_data, s_pic_slave_mask); 

    jlos_idt_pointer_t idt;
    idt.m_size = 256 * sizeof(jlos_gate_descriptor_t) - 1;
    idt.m_base = (uint32_t)jlos_interrupt_descriptor_table;
    __asm__ __volatile__("lidt %0" : : "m" (idt));
}

void jlos_interrupt_manager_destroy(jlos_interrupt_manager_t* self)
{
}

void jlos_interrupt_manager_activate(jlos_interrupt_manager_t* self)
{
    if (jlos_active_interrupt_manager != NULL) {
        jlos_interrupt_manager_deactivate(jlos_active_interrupt_manager);
    }
    jlos_active_interrupt_manager = self;
    __asm__("sti");
}

void jlos_interrupt_manager_deactivate(jlos_interrupt_manager_t* self)
{
    if (jlos_active_interrupt_manager == self) {
        jlos_active_interrupt_manager = NULL;
        __asm__("cli");
    }
}

uint32_t jlos_interrupt_manager_handle_interrupt(uint8_t m_interrupt, uint32_t m_esp)
{
    if (jlos_active_interrupt_manager != NULL) {
        return jlos_interrupt_manager_do_handle_interrupt(jlos_active_interrupt_manager, m_interrupt, m_esp);
    }
    return m_esp;
}

uint16_t jlos_interrupt_manager_hardware_interrupt_offset(jlos_interrupt_manager_t* self)
{
    return self->m_hardware_interrupt_offset;
}

uint32_t jlos_interrupt_manager_do_handle_interrupt(jlos_interrupt_manager_t* self, uint8_t m_interrupt, uint32_t m_esp)
{
    /* page fault 必须在 IRQ 向量转换之前处理，否则 0x0E 被偏移成 0x2E */
    if (m_interrupt == 0x0E) {
        jlos_irq_context_t context;
        jlos_irq_context_init(&context, m_esp);
        jlos_paging_page_fault_handler(&context);
        return m_esp;
    }

    uint8_t vector = m_interrupt;
    if (m_interrupt < 16) {
        vector = m_interrupt + self->m_hardware_interrupt_offset;
    }

    if (self->handles[vector] != NULL) {
        jlos_interrupt_handler_t *handler = (jlos_interrupt_handler_t*)self->handles[vector];
        m_esp = handler->handle_interrupt(handler, m_esp);
    }
    else if (m_interrupt >= 16) {
        jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)m_esp;
        printk("unhandled interrupt 0x%x, err = 0x%x, eip = %x, cs = %x, eflags = %x\n",
            m_interrupt, cpu->m_error, cpu->m_eip, cpu->m_cs, cpu->m_eflags);
    }

    /* IRQ0 (PIT): 先 tick 再调度，调度器读到最新 tick */
    if (m_interrupt == 0 && self != NULL && self->task_manager != NULL && self->task_manager->m_num_tasks > 0) {
        jlos_hal_timer_on_tick();
        jlos_task_t *curr = jlos_task_manager_curr_task_on_tick(self->task_manager);
#if KERNEL_CONFIG_PREEMPTIVE
        if (!curr || curr->m_remain_slice == 0 || curr->m_status != JLOS_TASK_RUNNING) {
            m_esp = (uint32_t)jlos_task_manager_schedule(self->task_manager, (jlos_cpu_state_t *)m_esp);
        }
#else
        if (!curr || curr->m_status != JLOS_TASK_RUNNING) {
            m_esp = (uint32_t)jlos_task_manager_schedule(self->task_manager, (jlos_cpu_state_t *)m_esp);
        }
#endif
    }
    if (m_interrupt < 16) {
        jlos_port8_bit_slow_write(&self->m_pic_master_command, 0x20);
        if (m_interrupt >= 8) {
            jlos_port8_bit_slow_write(&self->m_pic_slave_command, 0x20);
        }
    }
    return m_esp;
}

void jlos_interrupt_manager_register_handler(jlos_interrupt_manager_t* self, uint8_t m_interrupt, jlos_interrupt_handler_t* handler)
{
    self->handles[m_interrupt] = handler;
    jlos_irq_pic_unmask(self, m_interrupt);  /* 注册即 unmask */
}

void jlos_irq_context_init(jlos_irq_context_t *context, uint32_t arch_state_ptr)
{
    jlos_x86_regs_t *cpu = (jlos_x86_regs_t *)arch_state_ptr;
    context->m_error = cpu->m_error;
    context->m_instruction_pointer = cpu->m_eip;
    context->m_code_segment = cpu->m_cs;
    context->m_flags = cpu->m_eflags;
}