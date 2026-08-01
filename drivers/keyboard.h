#ifndef __DRIVERS_KEYBOARD_H
#define __DRIVERS_KEYBOARD_H

#include <common/types.h>
#include <hal/irq.h>
#include <hal/io.h>
#include <drivers/driver.h>

typedef struct jlos_keyboard_event_handler jlos_keyboard_event_handler_t;

struct jlos_keyboard_event_handler {
    void (*key_down)(jlos_keyboard_event_handler_t* self, char key);
    void (*on_key_up)(jlos_keyboard_event_handler_t* self, char key);
};

typedef struct jlos_keyboard_driver jlos_keyboard_driver_t;

struct jlos_keyboard_driver {
    jlos_driver_t base_driver;
    jlos_irq_handler_t base_handler;
    jlos_io8_t dataport;
    jlos_io8_t commandport;
    jlos_keyboard_event_handler_t *handler;
};

void jlos_keyboard_event_handler_init(jlos_keyboard_event_handler_t* self);
void jlos_keyboard_event_handler_key_down(jlos_keyboard_event_handler_t* self, char key);
void jlos_keyboard_event_handler_on_key_up(jlos_keyboard_event_handler_t* self, char key);

void jlos_keyboard_driver_init(jlos_keyboard_driver_t* self, jlos_irq_manager_t *manager, jlos_keyboard_event_handler_t *handler);
void jlos_keyboard_driver_destroy(jlos_keyboard_driver_t* self);
uint32_t jlos_keyboard_driver_handle_interrupt(jlos_keyboard_driver_t* self, uint32_t esp);
void jlos_keyboard_driver_activate(jlos_keyboard_driver_t* self);

#endif