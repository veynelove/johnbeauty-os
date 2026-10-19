/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_HAL_MMU_H
#define _JLOS_HAL_MMU_H

#include <common/types.h>

typedef struct jlos_mmu jlos_mmu_t;

extern void jlos_mmu_init(void);

extern uint16_t jlos_mmu_code_selector(jlos_mmu_t *self);
extern uint16_t jlos_mmu_data_selector(jlos_mmu_t *self);

extern uint16_t jlos_mmu_user_code_selector(jlos_mmu_t *self);
extern uint16_t jlos_mmu_user_data_selector(jlos_mmu_t *self);

extern jlos_mmu_t *jlos_mmu_get_kernel(void);

typedef struct {
  uintptr_t text_start;
  uintptr_t text_end;
  uintptr_t rodata_start;
  uintptr_t rodata_end;
  uintptr_t data_start;
  uintptr_t data_end;
  uintptr_t bss_start;
  uintptr_t bss_end;
  uintptr_t kernel_end;
} jlos_hal_kernel_segments_t;

const jlos_hal_kernel_segments_t *jlos_hal_get_kernel_segments(void);

#endif
