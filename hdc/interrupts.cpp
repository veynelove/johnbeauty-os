#include <hdc/interrupts.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printf_hex(uint8_t);
}

namespace JLOS {
namespace Hdc {
interrupt_handler::interrupt_handler(interrupt_manager *interrupt_manager_, uint8_t interrupt_number_)
{
	this->m_interrupt_number = interrupt_number_;
	this->m_interrupt_manager = interrupt_manager_;
	m_interrupt_manager->handles[m_interrupt_number] = this;
}

interrupt_handler::~interrupt_handler()
{
	if (m_interrupt_manager->handles[m_interrupt_number] == this) {
		m_interrupt_manager->handles[m_interrupt_number] = nullptr;
	}
}

uint32_t interrupt_handler::handle_interrupt(uint32_t m_esp)
{
	return m_esp;
}

interrupt_manager::gate_descriptor interrupt_manager::interrupt_descriptor_table[256];
interrupt_manager* interrupt_manager::activate_interrupt_manager = nullptr;

void interrupt_manager::set_interrupt_descriptor_table_entry(uint8_t m_interrupt_number,
	uint16_t code_segment_selector_offset, void (*handler)(), uint8_t descriptor_privilege_level,
	uint8_t descriptor_type)
{
	const uint8_t IDT_DESC_PRESENT = 0x80;
	
	interrupt_descriptor_table[m_interrupt_number].m_handle_address_low_bits =
		((uint32_t)handler) & 0xFFFF;
	interrupt_descriptor_table[m_interrupt_number].m_handle_address_high_bits =
		(((uint32_t)handler) >>16) & 0xFFFF;
	interrupt_descriptor_table[m_interrupt_number].m_gdt_codeSegmentSelector =
		code_segment_selector_offset;
	interrupt_descriptor_table[m_interrupt_number].m_access = (IDT_DESC_PRESENT | descriptor_type
		| ((descriptor_privilege_level&3) <<5));
	interrupt_descriptor_table[m_interrupt_number].m_reserved = 0;
}
		
interrupt_manager::interrupt_manager(uint16_t hardware_interruptoffset,
	Kernel::global_descriptor_table* gdt, Kernel::task_manager *task_manager) : m_pic_master_command(0x20),
	m_pic_master_data(0x21), m_pic_slave_command(0xA0), m_pic_slave_data(0xA1)
{
	this->task_manager = task_manager;
	this->m_hardware_interrupt_offset = hardware_interruptoffset;
	uint16_t code_segment = gdt->code_segment_selector();
	const uint8_t IDT_INTERRUPT_GATE = 0XE;
	for (uint16_t i = 0; i < 256; i++) {
		handles[i] = nullptr;
		set_interrupt_descriptor_table_entry(i, code_segment, &ignore_interrupt_request, 0,
			IDT_INTERRUPT_GATE);
	}
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset(), code_segment,
		&handle_interrupt_request0x00, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x01, code_segment,
		&handle_interrupt_request0x01, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x02, code_segment,
		&handle_interrupt_request0x02, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x03, code_segment,
		&handle_interrupt_request0x03, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x04, code_segment,
		&handle_interrupt_request0x04, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x05, code_segment,
		&handle_interrupt_request0x05, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x06, code_segment,
		&handle_interrupt_request0x06, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x07, code_segment,
		&handle_interrupt_request0x07, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x08, code_segment,
		&handle_interrupt_request0x08, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x09, code_segment,
		&handle_interrupt_request0x09, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x0A, code_segment,
		&handle_interrupt_request0x0a, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x0B, code_segment,
		&handle_interrupt_request0x0b, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x0C, code_segment,
		&handle_interrupt_request0x0c, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x0D, code_segment,
		&handle_interrupt_request0x0d, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x0E, code_segment,
		&handle_interrupt_request0x0e, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x0F, code_segment,
		&handle_interrupt_request0x0f, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(hardware_interrupt_offset() + 0x31, code_segment,
		&handle_interrupt_request0x31, 0, IDT_INTERRUPT_GATE);
	
	set_interrupt_descriptor_table_entry(					    0x80, code_segment,
		&handle_interrupt_request0x80, 0, IDT_INTERRUPT_GATE);

	set_interrupt_descriptor_table_entry(0x00, code_segment, &handle_exception0x00, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x01, code_segment, &handle_exception0x01, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x02, code_segment, &handle_exception0x02, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x03, code_segment, &handle_exception0x03, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x04, code_segment, &handle_exception0x04, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x05, code_segment, &handle_exception0x05, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x06, code_segment, &handle_exception0x06, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x07, code_segment, &handle_exception0x07, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x08, code_segment, &handle_exception0x08, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x09, code_segment, &handle_exception0x09, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x0A, code_segment, &handle_exception0x0a, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x0B, code_segment, &handle_exception0x0b, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x0C, code_segment, &handle_exception0x0c, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x0D, code_segment, &handle_exception0x0d, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x0E, code_segment, &handle_exception0x0e, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x0F, code_segment, &handle_exception0x0f, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x10, code_segment, &handle_exception0x10, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x11, code_segment, &handle_exception0x11, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x12, code_segment, &handle_exception0x12, 0, IDT_INTERRUPT_GATE);
	set_interrupt_descriptor_table_entry(0x13, code_segment, &handle_exception0x13, 0, IDT_INTERRUPT_GATE);

	m_pic_master_command.write(0x11);
	m_pic_slave_command.write(0x11);
	m_pic_master_data.write(hardware_interruptoffset);
	m_pic_slave_data.write(hardware_interruptoffset + 8);
	m_pic_master_data.write(0x04);
	m_pic_slave_data.write(0x02); 
	m_pic_master_data.write(0x01);
	m_pic_slave_data.write(0x01); 
	m_pic_master_data.write(0x00);
	m_pic_slave_data.write(0x00); 

	interrupt_descriptor_table_pointer idt;
	idt.m_size = 256 * sizeof(gate_descriptor) -1;
	idt.m_base = (uint32_t)interrupt_descriptor_table;
	asm volatile("lidt %0" : : "m" (idt));
}

interrupt_manager::~interrupt_manager(){}

void interrupt_manager::activate()
{
	if (activate_interrupt_manager != nullptr) {
		activate_interrupt_manager->deactivate();
	}
	activate_interrupt_manager = this;
	asm("sti");
}

void interrupt_manager::deactivate()
{
	if (activate_interrupt_manager == this) {
		activate_interrupt_manager = nullptr;
		asm("cli");
	}
}

uint32_t interrupt_manager::handle_interrupt(uint8_t m_interrupt, uint32_t m_esp)
{
	if (activate_interrupt_manager != nullptr) {
		return activate_interrupt_manager->do_handle_interrupt(m_interrupt, m_esp);
	}
    return m_esp;
}

uint16_t interrupt_manager::hardware_interrupt_offset()
{
	return m_hardware_interrupt_offset;
}

uint32_t interrupt_manager::do_handle_interrupt(uint8_t m_interrupt, uint32_t m_esp)
{
	if (handles[m_interrupt] != nullptr) {
		m_esp = handles[m_interrupt]->handle_interrupt(m_esp);
	}
	else if (m_interrupt != hardware_interrupt_offset()) {
		Kernel::printf("UNHANDLED INTERUPT 0x");
		Kernel::printf_hex(m_interrupt);
	}
	
	if (m_interrupt == hardware_interrupt_offset()) {
		m_esp = (uint32_t)task_manager->schedule((Kernel::cpu_state *)m_esp);
	}
	//hardware interrupts must be acknowledged
	if (hardware_interrupt_offset() <= m_interrupt && m_interrupt < hardware_interrupt_offset() + 16) {
		m_pic_master_command.write(0x20);
		if (hardware_interrupt_offset() + 8 <= m_interrupt)
			m_pic_slave_command.write(0x20);
	}
    return m_esp;
}
}
}
