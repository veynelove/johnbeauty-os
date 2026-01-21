#ifndef __GDT_H
#define __GDT_H

#include <common/types.h>

namespace JLOS {
namespace Kernel {
class global_descriptor_table {
public:
	class segment_descriptor {
	private:
		uint16_t m_limit_lo;
		uint16_t m_base_lo;
		uint8_t m_base_hi;
		uint8_t m_type;
		uint8_t m_flags_limit_hi;
		uint8_t m_base_vhi;
	public:
		segment_descriptor(uint32_t m_base, uint32_t limit, uint8_t m_type);
		uint32_t m_base();
		uint32_t limit();
	} __attribute__((packed));
public:
	segment_descriptor m_null_segment_selector;
	segment_descriptor m_unused_segment_selector;
	segment_descriptor m_code_segment_selector;
	segment_descriptor m_data_segment_selector;
public:
	global_descriptor_table();
	~global_descriptor_table();
	
	uint16_t code_segment_selector();
	uint16_t data_segment_selector();
};
}
}
#endif
