#ifndef _JLOS_KERNEL_PRINTK_H
#define _JLOS_KERNEL_PRINTK_H

#include <common/types.h>
#include <tools/config.h>

#define JLOS_KERNEL_LOG_EMERG  0
#define JLOS_KERNEL_LOG_ALERT  1
#define JLOS_KERNEL_LOG_CRIT   2
#define JLOS_KERNEL_LOG_ERR    3
#define JLOS_KERNEL_LOG_WARN   4
#define JLOS_KERNEL_LOG_NOTICE 5
#define JLOS_KERNEL_LOG_INFO   6
#define JLOS_KERNEL_LOG_DEBUG  7

void printf(const char *str);

void printk(int level, const char *subsys, const char *func, const char *fmt, ...);

#if JLOS_KERNEL_LOG_FUNC
 #define printk_func __func__
#else
 #define printk_func ((const char *)0)
#endif

#ifndef JLOS_KERNEL_LOG_SUBSYS
#define JLOS_KERNEL_LOG_SUBSYS "john_sunshine"
#endif

#define printk_emerg(fmt,...)  printk(JLOS_KERNEL_LOG_EMERG,  JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#define printk_alert(fmt,...)  printk(JLOS_KERNEL_LOG_ALERT,  JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#define printk_crit(fmt,...)  printk(JLOS_KERNEL_LOG_CRIT,   JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#define printk_err(fmt,...)   printk(JLOS_KERNEL_LOG_ERR,    JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#define printk_warn(fmt,...)  printk(JLOS_KERNEL_LOG_WARN,   JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#define printk_notice(fmt,...)printk(JLOS_KERNEL_LOG_NOTICE,JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#define printk_info(fmt,...)  printk(JLOS_KERNEL_LOG_INFO,   JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#if JLOS_KERNEL_DEBUG
 #define printk_debug(fmt,...) printk(JLOS_KERNEL_LOG_DEBUG,JLOS_KERNEL_LOG_SUBSYS, printk_func, fmt, ##__VA_ARGS__)
#else
 #define printk_debug(fmt,...) do{}while(0)
#endif

void jlos_printk_init(void);

#endif
