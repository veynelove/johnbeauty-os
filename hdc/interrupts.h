#ifndef __HDC_INTERUPTS_H
#define __HDC_INTERUPTS_H

#include <common/types.h>
#include <hdc/port.h>
#include <kernel/gdt.h>
#include <kernel/multitask.h>

namespace JLOS {
namespace Hdc {
class interrupt_manager;

class interrupt_handler {
public:
	virtual uint32_t handle_interrupt(uint32_t m_esp);
protected:
	uint8_t m_interrupt_number;
	interrupt_manager *m_interrupt_manager;

	interrupt_handler(interrupt_manager *interrupt_manager_, uint8_t interrupt_number_);
	~interrupt_handler();
};

class interrupt_manager {
friend class interrupt_handler;
protected:
	static interrupt_manager *activate_interrupt_manager;
	uint16_t m_hardware_interrupt_offset;
	interrupt_handler *handles[256];
	Kernel::task_manager *task_manager;

	struct gate_descriptor {
		uint16_t m_handle_address_low_bits;
		uint16_t m_gdt_codeSegmentSelector;
		uint8_t m_reserved;
		uint8_t m_access;
		uint16_t m_handle_address_high_bits;
	} __attribute__((packed));
	
	static gate_descriptor interrupt_descriptor_table[256];
	struct interrupt_descriptor_table_pointer {
		uint16_t m_size;
		uint32_t m_base;
	} __attribute__((packed));
	
	void set_interrupt_descriptor_table_entry(
		uint8_t interrupt_number_,
		uint16_t code_segment_selector_offset,
		void (*handler)(),
		uint8_t descriptor_privilege_level,
		uint8_t descriptor_type
	);

	port8_bit_slow m_pic_master_command;
	port8_bit_slow m_pic_master_data;
	port8_bit_slow m_pic_slave_command;
	port8_bit_slow m_pic_slave_data;

public:
	interrupt_manager(uint16_t hardware_interruptoffset, Kernel::global_descriptor_table* gdt,
		Kernel::task_manager *task_manager);
	~interrupt_manager();

	void activate();
	void deactivate();
	static uint32_t handle_interrupt(uint8_t m_interrupt, uint32_t m_esp);
	uint16_t hardware_interrupt_offset();
	uint32_t do_handle_interrupt(uint8_t m_interrupt, uint32_t m_esp);

	static void ignore_interrupt_request();
	
	static void handle_exception0x00();
     static void handle_exception0x01();
	static void handle_exception0x02();
	static void handle_exception0x03();
	static void handle_exception0x04();
	static void handle_exception0x05();
	static void handle_exception0x06();
	static void handle_exception0x07();
	static void handle_exception0x08();
	static void handle_exception0x09();
	static void handle_exception0x0a();
	static void handle_exception0x0b();
	static void handle_exception0x0c();
	static void handle_exception0x0d();
	static void handle_exception0x0e();
	static void handle_exception0x0f();
	static void handle_exception0x10();
	static void handle_exception0x11();
	static void handle_exception0x12();
	static void handle_exception0x13();

	static void handle_interrupt_request0x00();
	static void handle_interrupt_request0x01();
	static void handle_interrupt_request0x02();
	static void handle_interrupt_request0x03();
	static void handle_interrupt_request0x04();
	static void handle_interrupt_request0x05();
	static void handle_interrupt_request0x06();
	static void handle_interrupt_request0x07();
	static void handle_interrupt_request0x08();
	static void handle_interrupt_request0x09();
	static void handle_interrupt_request0x0a();
	static void handle_interrupt_request0x0b();
	static void handle_interrupt_request0x0c();
	static void handle_interrupt_request0x0d();
	static void handle_interrupt_request0x0e();
	static void handle_interrupt_request0x0f();
	static void handle_interrupt_request0x31();

	static void handle_interrupt_request0x80();
};
}
}
#endif
