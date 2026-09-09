#ifndef _JLOS_HAL_MMU_H
#define _JLOS_HAL_MMU_H

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

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

typedef jlos_gdt_t                    jlos_mmu_t;
typedef jlos_gdt_segment_descriptor_t jlos_mmu_segment_t;

extern void jlos_mmu_init();

extern uint16_t jlos_mmu_code_selector(jlos_mmu_t *self);
extern uint16_t jlos_mmu_data_selector(jlos_mmu_t *self);

extern uint16_t jlos_mmu_user_code_selector(jlos_mmu_t *self);
extern uint16_t jlos_mmu_user_data_selector(jlos_mmu_t *self);

extern void jlos_mmu_segment_init(jlos_mmu_segment_t *self, uint32_t base, uint32_t limit, uint8_t flags);
extern uint32_t jlos_mmu_segment_base(jlos_mmu_segment_t *self);
extern uint32_t jlos_mmu_segment_limit(jlos_mmu_segment_t *self);

extern jlos_mmu_t *jlos_mmu_get_kernel(void);
#endif

typedef struct {
  uint32_t text_start;
  uint32_t text_end;
  uint32_t rodata_start;
  uint32_t rodata_end;
  uint32_t data_start;
  uint32_t data_end;
  uint32_t bss_start;
  uint32_t bss_end;
  uint32_t kernel_end;
} jlos_hal_kernel_segments_t;

void jlos_hal_kernel_segments_init(void);
const jlos_hal_kernel_segments_t *jlos_hal_get_kernel_segments(void);
#endif
