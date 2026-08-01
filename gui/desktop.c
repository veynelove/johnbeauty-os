#include <gui/desktop.h>

void jlos_desktop_init(jlos_desktop_t* self, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b)
{
    jlos_composite_widget_init(&self->base_widget, NULL, 0, 0, w, h, r, g, b);
    self->base_widget.base_widget.draw = (void (*)(jlos_widget_t*, jlos_graphics_context_t*))jlos_desktop_draw;
    self->mouse_x = w / 2;
    self->mouse_y = h / 2;
}

void jlos_desktop_destroy(jlos_desktop_t* self)
{
    jlos_composite_widget_destroy(&self->base_widget);
}

void jlos_desktop_draw(jlos_desktop_t* self, jlos_graphics_context_t *gc)
{
    jlos_composite_widget_draw(&self->base_widget, gc);
    for (int i = 0; i < 4; i++) {
        jlos_graphics_context_put_pixel(gc, self->mouse_x - i, self->mouse_y, 0xFF, 0xFF, 0xFF);
        jlos_graphics_context_put_pixel(gc, self->mouse_x + i, self->mouse_y, 0xFF, 0xFF, 0xFF);
        jlos_graphics_context_put_pixel(gc, self->mouse_x, self->mouse_y - i, 0xFF, 0xFF, 0xFF);
        jlos_graphics_context_put_pixel(gc, self->mouse_x, self->mouse_y + i, 0xFF, 0xFF, 0xFF);
    }
}

void jlos_desktop_on_mouse_down(jlos_desktop_t* self, uint8_t button)
{
    jlos_composite_widget_on_mouse_down(&self->base_widget, self->mouse_x, self->mouse_y, button);
}

void jlos_desktop_on_mouse_up(jlos_desktop_t* self, uint8_t button)
{
    jlos_composite_widget_on_mouse_up(&self->base_widget, self->mouse_x, self->mouse_y, button);
}

void jlos_desktop_mouse_move(jlos_desktop_t* self, int32_t xoffset, int32_t yoffset)
{
    xoffset /= 4;
    yoffset /= 4;

    int32_t new_mouse_x = self->mouse_x + xoffset;
    if (new_mouse_x < 0) new_mouse_x = 0;
    if (new_mouse_x >= (int32_t)self->base_widget.base_widget.w) new_mouse_x = self->base_widget.base_widget.w - 1;

    int32_t new_mouse_y = self->mouse_y + yoffset;
    if (new_mouse_y < 0) new_mouse_y = 0;
    if (new_mouse_y >= (int32_t)self->base_widget.base_widget.h) new_mouse_y = self->base_widget.base_widget.h - 1;
    jlos_composite_widget_mouse_move(&self->base_widget, self->mouse_x, self->mouse_y, new_mouse_x, new_mouse_y);
    self->mouse_x = new_mouse_x;
    self->mouse_y = new_mouse_y;
}