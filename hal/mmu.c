#include <hal/mmu.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

void jlos_mmu_init(jlos_mmu_t *self)
{ jlos_gdt_init(self); }
void jlos_mmu_destroy(jlos_mmu_t *self)
{ jlos_gdt_destroy(self); }
uint16_t jlos_mmu_code_selector(jlos_mmu_t *self)
{ return jlos_gdt_code_segment_selector(self); }
uint16_t jlos_mmu_data_selector(jlos_mmu_t *self)
{ return jlos_gdt_data_segment_selector(self); }

void jlos_mmu_segment_init(jlos_mmu_segment_t *self, uint32_t m_base,
                           uint32_t limit, uint8_t m_flags)
{ jlos_gdt_segment_descriptor_init(self, m_base, limit, m_flags); }
uint32_t jlos_mmu_segment_base(jlos_mmu_segment_t *self)
{ return jlos_gdt_segment_descriptor_base(self); }
uint32_t jlos_mmu_segment_limit(jlos_mmu_segment_t *self)
{ return jlos_gdt_segment_descriptor_limit(self); }

#endif
