#ifndef _JLOS_HAL_BARRIER_H
#define _JLOS_HAL_BARRIER_H

#include <common/types.h>

/* 编译器屏障：阻止 GCC 跨点重排访存，不发硬件指令 */
#define jlos_barrier()  __asm__ __volatile__("" ::: "memory")

extern void jlos_mb(void);
extern void jlos_rmb(void);
extern void jlos_wmb(void);

#endif
