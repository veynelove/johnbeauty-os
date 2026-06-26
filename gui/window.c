#include <gui/window.h>

void jlos_window_init(jlos_window_t* self, jlos_widget_t *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h, uint8_t m_r, uint8_t m_g, uint8_t m_b)
{
    jlos_composite_widget_init(&self->base_widget, parent, m_x, m_y, m_w, m_h, m_r, m_g, m_b);
    self->base_widget.base_widget.on_mouse_down = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_window_on_mouse_down;
    self->base_widget.base_widget.on_mouse_up = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_window_on_mouse_up;
    self->base_widget.base_widget.mouse_move = (void (*)(jlos_widget_t*, int32_t, int32_t, int32_t, int32_t))jlos_window_mouse_move;
    self->m_dragging = false;
}

void jlos_window_destroy(jlos_window_t* self)
{
    jlos_composite_widget_destroy(&self->base_widget);
}

void jlos_window_on_mouse_down(jlos_window_t* self, int32_t m_x, int32_t m_y, uint8_t button)
{
    self->m_dragging = (button == 1);
    jlos_composite_widget_on_mouse_down(&self->base_widget, m_x, m_y, button);
}

void jlos_window_on_mouse_up(jlos_window_t* self, int32_t m_x, int32_t m_y, uint8_t button)
{
    self->m_dragging = false;
    jlos_composite_widget_on_mouse_up(&self->base_widget, m_x, m_y, button);
}

void jlos_window_mouse_move(jlos_window_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
    if (self->m_dragging) {
        self->base_widget.base_widget.m_x += newx - oldx;
        self->base_widget.base_widget.m_y += newy - oldy;
    }
    jlos_composite_widget_mouse_move(&self->base_widget, oldx, oldy, newx, newy);
}