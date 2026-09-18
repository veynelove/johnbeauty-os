#ifndef _JLOS_ARCH_X86_INTERRUPTS_H
#define _JLOS_ARCH_X86_INTERRUPTS_H

#include <common/types.h>
#include <hal/io.h>
#include <hal/irq.h>
#include <kernel/multitask.h>

#define KERNEL_FIRST_INTERRUPT_VECTOR 0x20

struct jlos_irq_manager {
    uint16_t                hardware_interrupt_offset;
    void                    *handles[256];
    jlos_task_manager_t     *task_manager;
    jlos_io8_slow_t         pic_master_command;
    jlos_io8_slow_t         pic_master_data;
    jlos_io8_slow_t         pic_slave_command;
    jlos_io8_slow_t         pic_slave_data;
};

typedef struct {
    uint16_t    handle_address_low_bits;
    uint16_t    gdt_codeSegmentSelector;
    uint8_t     reserved;
    uint8_t     access;
    uint16_t    handle_address_high_bits;
} __attribute__((packed)) jlos_gate_descriptor_t;

void jlos_handle_exception0x00();
void jlos_handle_exception0x01();
void jlos_handle_exception0x02();
void jlos_handle_exception0x03();
void jlos_handle_exception0x04();
void jlos_handle_exception0x05();
void jlos_handle_exception0x06();
void jlos_handle_exception0x07();
void jlos_handle_exception0x08();
void jlos_handle_exception0x09();
void jlos_handle_exception0x0a();
void jlos_handle_exception0x0b();
void jlos_handle_exception0x0c();
void jlos_handle_exception0x0d();
void jlos_handle_exception0x0e();
void jlos_handle_exception0x0f();
void jlos_handle_exception0x10();
void jlos_handle_exception0x11();
void jlos_handle_exception0x12();
void jlos_handle_exception0x13();

void jlos_handle_interrupt_request0x00();
void jlos_handle_interrupt_request0x01();
void jlos_handle_interrupt_request0x02();
void jlos_handle_interrupt_request0x03();
void jlos_handle_interrupt_request0x04();
void jlos_handle_interrupt_request0x05();
void jlos_handle_interrupt_request0x06();
void jlos_handle_interrupt_request0x07();
void jlos_handle_interrupt_request0x08();
void jlos_handle_interrupt_request0x09();
void jlos_handle_interrupt_request0x0a();
void jlos_handle_interrupt_request0x0b();
void jlos_handle_interrupt_request0x0c();
void jlos_handle_interrupt_request0x0d();
void jlos_handle_interrupt_request0x0e();
void jlos_handle_interrupt_request0x0f();
void jlos_handle_interrupt_request0x31();
void jlos_handle_interrupt_request0x80();

#endif
