#ifndef __JLOS_HAL_MMU_H
#define __JLOS_HAL_MMU_H

#include <tools/config.h>
#include <common/types.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/gdt.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture MMU support not implemented yet"
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_RISCV
#error "RISC-V architecture MMU support not implemented yet"
#else
#error "Unknown KERNEL_CONFIG_HARDWARE_ARCH value"
#endif

/* MMU 统一命名：x86 GDT / ARM TTBR0+MAIR / RISC-V satp 上层透明 */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_gdt_t                    jlos_mmu_t;
  typedef jlos_gdt_segment_descriptor_t jlos_mmu_segment_t;

  extern void jlos_mmu_init(jlos_mmu_t *self);
  extern void jlos_mmu_destroy(jlos_mmu_t *self);
  extern uint16_t jlos_mmu_code_selector(jlos_mmu_t *self);
  extern uint16_t jlos_mmu_data_selector(jlos_mmu_t *self);

  extern void jlos_mmu_segment_init(jlos_mmu_segment_t *self, uint32_t m_base,
                                    uint32_t limit, uint8_t m_flags);
  extern uint32_t jlos_mmu_segment_base(jlos_mmu_segment_t *self);
  extern uint32_t jlos_mmu_segment_limit(jlos_mmu_segment_t *self);
#endif

#endif
