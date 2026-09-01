#ifndef __JLOS_ARCH_X86_FPU_STATE_H
#define __JLOS_ARCH_X86_FPU_STATE_H

#include <common/types.h>

#define JLOS_ARCH_X86_FXSAVE_AREA_SIZE 512

// FXSAVE 512B 区域布局：x87 状态(128B) + SSE XMM0-XMM7(128B) + MXCSR + 保留
// 要求 16B 对齐。这里用 page_frame_malloc 天然 4KB 对齐，满足要求。
// Floating-point/Streaming SIMD Extension State Save
struct jlos_arch_ext_state {
    uint8_t fxsave_area[JLOS_ARCH_X86_FXSAVE_AREA_SIZE] __attribute__((aligned(16)));
    bool used;
};

#endif
