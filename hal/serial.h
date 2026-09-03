#ifndef _JLOS_HAL_SERIAL_H
#define _JLOS_HAL_SERIAL_H

#include <tools/config.h>
#include <common/types.h>

/* 默认 COM1 38400 8N1。参数可在 init 时指定，未来做 QEMU debugcon / 16550A / USB serial 切换时保持 API 不变。 */
#define JLOS_HAL_SERIAL_DEFAULT_COM   0x3F8
#define JLOS_HAL_SERIAL_DEFAULT_BAUD  38400

/* 单字符 / 字符串写（16550 LSR 5 bit 为 1 才写，避免丢字符） */
void jlos_hal_serial_putc(uint16_t com_base, char c);
void jlos_hal_serial_puts(uint16_t com_base, const char *s);

/* 常用简化版：使用默认 COM 口。 */
void jlos_hal_serial_default_init(void);
void jlos_hal_serial_default_putc(char c);
void jlos_hal_serial_default_puts(const char *s);

#endif
