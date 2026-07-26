#ifndef JLOS_KERNEL_PRINTK_H
#define JLOS_KERNEL_PRINTK_H

#include <common/types.h>

void printf(const char *str);
void printf_char(char c);
void printf_hex(uint8_t value);
void printf_hex16(uint16_t value);
void printf_hex32(uint32_t value);

void sysprintf(char *str);

void printk(const char *fmt, ...);

void jlos_printk_init(void);

#endif
