#ifndef __JLOS_GUI_WINDOW_H
#define __JLOS_GUI_WINDOW_H

#include <gui/widget.h>

typedef struct jlos_window jlos_window_t;

struct jlos_window {
    jlos_composite_widget_t base_widget;
    bool dragging;
};

void jlos_window_init(jlos_window_t* self, jlos_widget_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b);
void jlos_window_destroy(jlos_window_t* self);
void jlos_window_on_mouse_down(jlos_window_t* self, int32_t x, int32_t y, uint8_t button);
void jlos_window_on_mouse_up(jlos_window_t* self, int32_t x, int32_t y, uint8_t button);
void jlos_window_mouse_move(jlos_window_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);

#endif