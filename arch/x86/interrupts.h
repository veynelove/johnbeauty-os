#ifndef _HDC_INTERUPTS_H
#define _HDC_INTERUPTS_H

#include <common/types.h>
#include <arch/x86/port.h>
#include <arch/x86/gdt.h>
#include <kernel/multitask.h>

typedef struct jlos_interrupt_manager jlos_interrupt_manager_t;
typedef struct jlos_interrupt_handler jlos_interrupt_handler_t;

typedef uint32_t (*jlos_interrupt_handler_func_t)(jlos_interrupt_handler_t*, uint32_t);

struct jlos_interrupt_handler {
    uint8_t interrupt_number;
    jlos_interrupt_manager_t *interrupt_manager;
    jlos_interrupt_handler_func_t handle_interrupt;
};

struct jlos_interrupt_manager {
    uint16_t hardware_interrupt_offset;
    void *handles[256];
    jlos_task_manager_t *task_manager;

    jlos_port8_bit_slow_t pic_master_command;
    jlos_port8_bit_slow_t pic_master_data;
    jlos_port8_bit_slow_t pic_slave_command;
    jlos_port8_bit_slow_t pic_slave_data;
};

extern jlos_interrupt_manager_t *jlos_active_interrupt_manager;

void jlos_interrupt_handler_init(jlos_interrupt_handler_t* self, jlos_interrupt_manager_t *interrupt_manager, uint8_t interrupt_number);
void jlos_interrupt_handler_destroy(jlos_interrupt_handler_t* self);
uint32_t jlos_interrupt_handler_handle_interrupt(jlos_interrupt_handler_t* self, uint32_t esp);

void jlos_interrupt_manager_init(jlos_interrupt_manager_t* self, uint16_t hardware_interruptoffset, jlos_gdt_t* gdt, jlos_task_manager_t *task_manager);

void jlos_interrupt_manager_activate(jlos_interrupt_manager_t* self);
void jlos_interrupt_manager_deactivate(jlos_interrupt_manager_t* self);
uint32_t jlos_interrupt_manager_handle_interrupt(uint8_t interrupt, uint32_t esp);
uint16_t jlos_interrupt_manager_hardware_interrupt_offset(jlos_interrupt_manager_t* self);
uint32_t jlos_interrupt_manager_do_handle_interrupt(jlos_interrupt_manager_t* self, uint8_t interrupt, uint32_t esp);
void jlos_interrupt_manager_register_handler(jlos_interrupt_manager_t* self, uint8_t interrupt, jlos_interrupt_handler_t* handler);

void jlos_ignore_interrupt_request();

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