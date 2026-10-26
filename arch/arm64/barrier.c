/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/barrier.h>

inline void jlos_mb(void)  { __asm__ __volatile__("dmb sy" ::: "memory"); }

inline void jlos_rmb(void) { __asm__ __volatile__("dmb ld" ::: "memory"); }

inline void jlos_wmb(void) { __asm__ __volatile__("dmb st" ::: "memory"); }
