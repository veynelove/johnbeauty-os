#ifndef __JLOS_DEBUG_CONSOLE_H
#define __JLOS_DEBUG_CONSOLE_H

#include <hdc/interrupts.h>
#include <drivers/driver.h>

void debug_console_init(jlos_interrupt_manager_t *interrupts, jlos_driver_manager_t *driver_manager_);
void debug_console_mouse(jlos_interrupt_manager_t *interrupts, jlos_driver_manager_t *driver_manager_);
void debug_console_keyboard(jlos_interrupt_manager_t *interrupts, jlos_driver_manager_t *driver_manager_);

#endif
