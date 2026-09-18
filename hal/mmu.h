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

const jlos_hal_kernel_segments_t *jlos_hal_get_kernel_segments(void);

#endif
