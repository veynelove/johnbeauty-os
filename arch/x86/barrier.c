#include <hal/barrier.h>

#define HAL_CONFIG_HAS_MB    1
#define HAL_CONFIG_HAS_RMB   1
#define HAL_CONFIG_HAS_WMB   1

/* 全屏障：LoadLoad + LoadStore + StoreLoad + StoreStore */
inline void jlos_mb(void) { __asm__ __volatile__("mfence" ::: "memory"); }

/* 读屏障：LoadLoad（x86 TSO 下基本只需编译器屏障，仍保留 lfence 以兼容 WC/UC 内存） */
inline void jlos_rmb(void) { __asm__ __volatile__("lfence" ::: "memory"); }

/* 写屏障：StoreStore（x86 TSO 下基本只需编译器屏障，仍保留 sfence 以兼容 WC/NT store） */
inline void jlos_wmb(void) { __asm__ __volatile__("sfence" ::: "memory"); }
