/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_X86_AP_BOOT_H
#define _JLOS_ARCH_X86_AP_BOOT_H

#include <common/types.h>

/* INIT-SIPI-SIPI 时序 (Intel SDM Vol.3 §8.4):
 * INIT 后等 ≥10ms, SIPI 间等 ≥200µs。按 TSC 周期计, 取保守上界。 */
#define JLOS_AP_INIT_DELAY_CYCLES     50000000
#define JLOS_AP_SIPI_DELAY_CYCLES     1000000
#define JLOS_AP_ONLINE_TIMEOUT        100000000

/* AP 启动入口: AP 经 trampoline 切到高半核后跳入此 C 函数 */
void jlos_ap_main(void);

/* HAL 层: 启动所有 AP (BSP 调用) */
void jlos_hal_smp_boot_aps(void);

#endif
