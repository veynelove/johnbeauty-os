#ifndef _JLOS_HAL_SERIAL_H
#define _JLOS_HAL_SERIAL_H

#include <common/types.h>

void jlos_hal_serial_putc(uint16_t com_base, char c);
void jlos_hal_serial_puts(uint16_t com_base, const char *s);

void jlos_hal_serial_default_init(void);
void jlos_hal_serial_default_putc(char c);
void jlos_hal_serial_default_puts(const char *s);

#endif
