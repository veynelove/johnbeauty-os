#ifndef __JLOS_GUI_DESKTOP_H
#define __JLOS_GUI_DESKTOP_H

#include <gui/widget.h>

typedef struct jlos_desktop jlos_desktop_t;

struct jlos_desktop {
    jlos_composite_widget_t base_widget;
    uint32_t mouse_x;
    uint32_t mouse_y;
};

void jlos_desktop_init(jlos_desktop_t* self, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b);
void jlos_desktop_destroy(jlos_desktop_t* self);
void jlos_desktop_draw(jlos_desktop_t* self, jlos_graphics_context_t *gc);
void jlos_desktop_on_mouse_down(jlos_desktop_t* self, uint8_t button);
void jlos_desktop_on_mouse_up(jlos_desktop_t* self, uint8_t button);
void jlos_desktop_mouse_move(jlos_desktop_t* self, int32_t xoffset, int32_t yoffset);

#endif