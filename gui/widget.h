#ifndef __JLOS_GUI_WIDGET_H
#define __JLOS_GUI_WIDGET_H

#include <common/types.h>
#include <common/graphics.h>

typedef struct jlos_widget jlos_widget_t;

struct jlos_widget {
    jlos_widget_t *parent;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool focussable;
    void (*get_focus)(jlos_widget_t* self, jlos_widget_t *widget);
    void (*model_to_screen)(jlos_widget_t* self, int32_t *x, int32_t *y);
    bool (*contains_coordinate)(jlos_widget_t* self, int32_t x, int32_t y);
    void (*draw)(jlos_widget_t* self, jlos_graphics_context_t *gc);
    void (*on_mouse_down)(jlos_widget_t* self, int32_t x, int32_t y, uint8_t button);
    void (*on_mouse_up)(jlos_widget_t* self, int32_t x, int32_t y, uint8_t button);
    void (*mouse_move)(jlos_widget_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);
    void (*key_down)(jlos_widget_t* self, char c);
    void (*on_key_up)(jlos_widget_t* self, char c);
};

typedef struct jlos_composite_widget jlos_composite_widget_t;

struct jlos_composite_widget {
    jlos_widget_t base_widget;
    jlos_widget_t *children[100];
    int num_children;
    jlos_widget_t *focussed_child;
};

void jlos_widget_init(jlos_widget_t* self, jlos_widget_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b);
void jlos_widget_destroy(jlos_widget_t* self);
void jlos_widget_get_focus(jlos_widget_t* self, jlos_widget_t *widget);
void jlos_widget_model_to_screen(jlos_widget_t* self, int32_t *x, int32_t *y);
bool jlos_widget_contains_coordinate(jlos_widget_t* self, int32_t x, int32_t y);
void jlos_widget_draw(jlos_widget_t* self, jlos_graphics_context_t *gc);
void jlos_widget_on_mouse_down(jlos_widget_t* self, int32_t x, int32_t y, uint8_t button);
void jlos_widget_on_mouse_up(jlos_widget_t* self, int32_t x, int32_t y, uint8_t button);
void jlos_widget_mouse_move(jlos_widget_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);

void jlos_composite_widget_init(jlos_composite_widget_t* self, jlos_widget_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b);
void jlos_composite_widget_destroy(jlos_composite_widget_t* self);
void jlos_composite_widget_get_focus(jlos_composite_widget_t* self, jlos_widget_t *widget);
bool jlos_composite_widget_add_child(jlos_composite_widget_t* self, jlos_widget_t *child);
void jlos_composite_widget_draw(jlos_composite_widget_t* self, jlos_graphics_context_t *gc);
void jlos_composite_widget_on_mouse_down(jlos_composite_widget_t* self, int32_t x, int32_t y, uint8_t button);
void jlos_composite_widget_on_mouse_up(jlos_composite_widget_t* self, int32_t x, int32_t y, uint8_t button);
void jlos_composite_widget_mouse_move(jlos_composite_widget_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);
void jlos_composite_widget_onkey_down(jlos_composite_widget_t* self, char c);
void jlos_composite_widget_onkey_up(jlos_composite_widget_t* self, char c);

#endif