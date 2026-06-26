#ifndef __JLOS_GUI_WINDOW_H
#define __JLOS_GUI_WINDOW_H

#include <gui/widget.h>

typedef struct jlos_window jlos_window_t;

struct jlos_window {
    jlos_composite_widget_t base_widget;
    bool m_dragging;
};

void jlos_window_init(jlos_window_t* self, jlos_widget_t *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h, uint8_t m_r, uint8_t m_g, uint8_t m_b);
void jlos_window_destroy(jlos_window_t* self);
void jlos_window_on_mouse_down(jlos_window_t* self, int32_t m_x, int32_t m_y, uint8_t button);
void jlos_window_on_mouse_up(jlos_window_t* self, int32_t m_x, int32_t m_y, uint8_t button);
void jlos_window_mouse_move(jlos_window_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);

#endif