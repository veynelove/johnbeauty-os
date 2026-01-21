#include <kernel/gdt.h>

namespace JLOS {
namespace Kernel {
global_descriptor_table::global_descriptor_table() : m_null_segment_selector(0,0,0),
m_unused_segment_selector(0,0,0), m_code_segment_selector(0,64*1024*1024,0x9A),
m_data_segment_selector(0,64*1024*1024,0x92)
{
	uint32_t i[2];
	i[1] = (uint32_t)this;
     i[0] = sizeof(global_descriptor_table) << 16;
	
	asm volatile("lgdt (%0)": : "p" (((uint8_t *) i)+2));
}

global_descriptor_table::~global_descriptor_table(){}

uint16_t global_descriptor_table::data_segment_selector()
{
	return (uint8_t*)&m_data_segment_selector - (uint8_t*)this;
}

uint16_t global_descriptor_table::code_segment_selector()
{
	return (uint8_t*)&m_code_segment_selector - (uint8_t*)this;
}

global_descriptor_table::segment_descriptor::segment_descriptor(uint32_t m_base, uint32_t limit,
	uint8_t m_flags)
{
	uint8_t* target = (uint8_t*)this;
	if (limit <= 65536) {
		target[6] = 0x40;
	} 
	else {
		if ((limit & 0xFFF) != 0xFFF) {
			limit = (limit >> 12) -1;
		}
		else {
			limit = limit >> 12;
		}
		target[6] = 0xC0;
	}
	target[0] = limit & 0xFF;
	target[1] = (limit >> 8) & 0xFF;
	target[6] |= (limit >> 16) & 0xF;
	
	target[2] = m_base & 0xFF;
	target[3] = (m_base >> 8) & 0xFF;
	target[4] = (m_base >> 16) & 0xFF;
	target[7] = (m_base >> 24) & 0xFF;
	
	target[5] = m_flags;
}

uint32_t global_descriptor_table::segment_descriptor::m_base()
{
	uint8_t *target = (uint8_t*)this;
	uint32_t result = target[7];
	result = (result << 8) + target[4];
	result = (result << 8) + target[3];
	result = (result << 8) + target[2];
	return result;
}

uint32_t global_descriptor_table::segment_descriptor::limit()
{
	uint8_t *target = (uint8_t*)this;
	uint32_t result = target[6] & 0xF;
	result = (result << 8) + target[1];
	result = (result << 8) + target[0];
	
	if ((target[6] & 0xC0) == 0xC0) {
		result = (result << 12) | 0xFFF;
	}
	return result;
}
}
}
