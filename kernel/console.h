#ifndef _JLOS_KERNEL_CONSOLE_H
#define _JLOS_KERNEL_CONSOLE_H

#include <common/types.h>

#define JLOS_CONSOLE_DEFAULT_COLS 80
#define JLOS_CONSOLE_DEFAULT_ROWS 25

void jlos_console_init(void);
void jlos_console_reinit(void);

void jlos_console_putc(char c);
void jlos_console_puts(const char *str);
void jlos_console_clear(void);
void jlos_console_set_attr(uint8_t attr);
uint8_t jlos_console_get_attr(void);
void jlos_console_get_cursor(uint32_t *col, uint32_t *row);
void jlos_console_set_cursor(uint32_t col, uint32_t row);
uint32_t jlos_console_get_cols(void);
uint32_t jlos_console_get_rows(void);
void jlos_console_lock(uint32_t *flags);
void jlos_console_unlock(uint32_t flags);

#endif
