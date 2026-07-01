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

/* ============================================================
 *  Unified Type Naming (ARM-style: mmu instead of x86-specific gdt)
 * ============================================================ */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_gdt_t                    jlos_mmu_t;
  typedef jlos_gdt_segment_descriptor_t jlos_mmu_segment_t;

  #define jlos_mmu_init(self)              jlos_gdt_init(self)
  #define jlos_mmu_destroy(self)           jlos_gdt_destroy(self)
  #define jlos_mmu_code_selector(self)     jlos_gdt_code_segment_selector(self)
  #define jlos_mmu_data_selector(self)     jlos_gdt_data_segment_selector(self)
  #define jlos_mmu_segment_init(...)       jlos_gdt_segment_descriptor_init(__VA_ARGS__)
  #define jlos_mmu_segment_base(seg)       jlos_gdt_segment_descriptor_base(seg)
  #define jlos_mmu_segment_limit(seg)      jlos_gdt_segment_descriptor_limit(seg)
#endif

/* ============================================================
 *  Unified API Contract
 *
 *  Any architecture port MUST provide:
 *    TYPES:
 *      - jlos_mmu_t            : MMU context (opaque, arch-specific)
 *      - jlos_mmu_segment_t    : Segment / page-table descriptor
 *
 *    FUNCTIONS:
 *      - void jlos_mmu_init(jlos_mmu_t *self)
 *      - void jlos_mmu_destroy(jlos_mmu_t *self)
 *      - uint16_t jlos_mmu_code_selector(jlos_mmu_t *self)
 *      - uint16_t jlos_mmu_data_selector(jlos_mmu_t *self)
 * ============================================================ */

#endif
