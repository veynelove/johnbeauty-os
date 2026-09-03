#ifndef _DRIVERS_MOUSE_H
#define _DRIVERS_MOUSE_H

#include <common/types.h>
#include <hal/irq.h>
#include <hal/io.h>
#include <drivers/driver.h>

typedef struct jlos_mouse_event_handler jlos_mouse_event_handler_t;

struct jlos_mouse_event_handler {
    void (*on_activate)(jlos_mouse_event_handler_t* self);
    void (*on_mouse_down)(jlos_mouse_event_handler_t* self, uint8_t button);
    void (*on_mouse_up)(jlos_mouse_event_handler_t* self, uint8_t button);
    void (*mouse_move)(jlos_mouse_event_handler_t* self, int32_t xoffset, int32_t yoffset);
};

typedef struct jlos_mouse_driver jlos_mouse_driver_t;

struct jlos_mouse_driver {
    jlos_driver_t base_driver;
    jlos_irq_handler_t base_handler;
    jlos_io8_t dataport;
    jlos_io8_t commandport;
    uint8_t buffer[3];
    uint8_t offset;
    uint8_t buttons;
    jlos_mouse_event_handler_t *handler;
};

void jlos_mouse_event_handler_init(jlos_mouse_event_handler_t* self);
void jlos_mouse_event_handler_on_activate(jlos_mouse_event_handler_t* self);
void jlos_mouse_event_handler_on_mouse_down(jlos_mouse_event_handler_t* self, uint8_t button);
void jlos_mouse_event_handler_on_mouse_up(jlos_mouse_event_handler_t* self, uint8_t button);
void jlos_mouse_event_handler_mouse_move(jlos_mouse_event_handler_t* self, int32_t xoffset, int32_t yoffset);

void jlos_mouse_driver_init(jlos_mouse_driver_t* self, jlos_irq_manager_t *manager, jlos_mouse_event_handler_t *handler);
void jlos_mouse_driver_destroy(jlos_mouse_driver_t* self);
uint32_t jlos_mouse_driver_handle_interrupt(jlos_mouse_driver_t* self, uint32_t esp);
void jlos_mouse_driver_activate(jlos_mouse_driver_t* self);

#endif