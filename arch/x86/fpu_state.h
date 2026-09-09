#ifndef _JLOS_ARCH_X86_FPU_STATE_H
#define _JLOS_ARCH_X86_FPU_STATE_H

#include <common/types.h>

#define JLOS_ARCH_X86_FXSAVE_AREA_SIZE 512

struct jlos_arch_ext_state {
    uint8_t fxsave_area[JLOS_ARCH_X86_FXSAVE_AREA_SIZE] __attribute__((aligned(16)));
    bool used;
};

#endif
