#include <gui/window.h>

void jlos_window_init(jlos_window_t* self, jlos_widget_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b)
{
    jlos_composite_widget_init(&self->base_widget, parent, x, y, w, h, r, g, b);
    self->base_widget.base_widget.on_mouse_down = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_window_on_mouse_down;
    self->base_widget.base_widget.on_mouse_up = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_window_on_mouse_up;
    self->base_widget.base_widget.mouse_move = (void (*)(jlos_widget_t*, int32_t, int32_t, int32_t, int32_t))jlos_window_mouse_move;
    self->dragging = false;
}

void jlos_window_destroy(jlos_window_t* self)
{
    jlos_composite_widget_destroy(&self->base_widget);
}

void jlos_window_on_mouse_down(jlos_window_t* self, int32_t x, int32_t y, uint8_t button)
{
    self->dragging = (button == 1);
    jlos_composite_widget_on_mouse_down(&self->base_widget, x, y, button);
}

void jlos_window_on_mouse_up(jlos_window_t* self, int32_t x, int32_t y, uint8_t button)
{
    self->dragging = false;
    jlos_composite_widget_on_mouse_up(&self->base_widget, x, y, button);
}

void jlos_window_mouse_move(jlos_window_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
    if (self->dragging) {
        self->base_widget.base_widget.x += newx - oldx;
        self->base_widget.base_widget.y += newy - oldy;
    }
    jlos_composite_widget_mouse_move(&self->base_widget, oldx, oldy, newx, newy);
}