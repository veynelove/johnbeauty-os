#ifndef __JLOS_HAL_BARRIER_H
#define __JLOS_HAL_BARRIER_H

#include <tools/config.h>
#include <common/types.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

/* 编译器屏障：阻止 GCC 跨点重排访存，不发硬件指令 */
#define jlos_barrier()  __asm__ __volatile__("" ::: "memory")

/* 全屏障：LoadLoad + LoadStore + StoreLoad + StoreStore */
static inline void jlos_mb(void) { __asm__ __volatile__("mfence" ::: "memory"); }

/* 读屏障：LoadLoad（x86 TSO 下基本只需编译器屏障，仍保留 lfence 以兼容 WC/UC 内存） */
static inline void jlos_rmb(void) { __asm__ __volatile__("lfence" ::: "memory"); }

/* 写屏障：StoreStore（x86 TSO 下基本只需编译器屏障，仍保留 sfence 以兼容 WC/NT store） */
static inline void jlos_wmb(void) { __asm__ __volatile__("sfence" ::: "memory"); }

#define HAL_CONFIG_HAS_MB    1
#define HAL_CONFIG_HAS_RMB   1
#define HAL_CONFIG_HAS_WMB   1

#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM

#define jlos_barrier()  __asm__ __volatile__("" ::: "memory")

static inline void jlos_mb(void)   { __asm__ __volatile__("dmb ish" ::: "memory"); }
static inline void jlos_rmb(void)  { __asm__ __volatile__("dmb ishld" ::: "memory"); }
static inline void jlos_wmb(void)  { __asm__ __volatile__("dmb ishst" ::: "memory"); }

#define HAL_CONFIG_HAS_MB    1
#define HAL_CONFIG_HAS_RMB   1
#define HAL_CONFIG_HAS_WMB   1

#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_RISCV

#define jlos_barrier()  __asm__ __volatile__("" ::: "memory")

static inline void jlos_mb(void)   { __asm__ __volatile__("fence rw, rw" ::: "memory"); }
static inline void jlos_rmb(void)  { __asm__ __volatile__("fence r, r"   ::: "memory"); }
static inline void jlos_wmb(void)  { __asm__ __volatile__("fence w, w"   ::: "memory"); }

#define HAL_CONFIG_HAS_MB    1
#define HAL_CONFIG_HAS_RMB   1
#define HAL_CONFIG_HAS_WMB   1

#else
#error "Unknown KERNEL_CONFIG_HARDWARE_ARCH value"
#endif

#endif
