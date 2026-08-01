#include <gui/widget.h>

void jlos_widget_init(jlos_widget_t* self, jlos_widget_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b)
{
    self->parent = parent;
    self->x = x;
    self->y = y;
    self->w = w;
    self->h = h;
    self->r = r;
    self->g = g;
    self->b = b;
    self->focussable = true;
    self->get_focus = jlos_widget_get_focus;
    self->model_to_screen = jlos_widget_model_to_screen;
    self->contains_coordinate = jlos_widget_contains_coordinate;
    self->draw = jlos_widget_draw;
    self->on_mouse_down = jlos_widget_on_mouse_down;
    self->on_mouse_up = jlos_widget_on_mouse_up;
    self->mouse_move = jlos_widget_mouse_move;
}

void jlos_widget_destroy(jlos_widget_t* self)
{
}

void jlos_widget_get_focus(jlos_widget_t* self, jlos_widget_t *widget)
{
    if (self->parent != NULL) {
        self->parent->get_focus(self->parent, widget);
    }
}

void jlos_widget_model_to_screen(jlos_widget_t* self, int32_t *x, int32_t *y)
{
    if (self->parent != NULL) {
        self->parent->model_to_screen(self->parent, x, y);
    }
    *x += self->x;
    *y += self->y;
}

void jlos_widget_draw(jlos_widget_t* self, jlos_graphics_context_t *gc)
{
    int X = 0;
    int Y = 0;
    self->model_to_screen(self, &X, &Y);
    jlos_graphics_context_fill_rectangle(gc, X, Y, self->w, self->h, self->r, self->g, self->b);
}

void jlos_widget_on_mouse_down(jlos_widget_t* self, int32_t x, int32_t y, uint8_t button)
{
    if (self->focussable) {
        self->get_focus(self, self);
    }
}

void jlos_widget_on_mouse_up(jlos_widget_t* self, int32_t x, int32_t y, uint8_t button)
{
}

void jlos_widget_mouse_move(jlos_widget_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
}

bool jlos_widget_contains_coordinate(jlos_widget_t* self, int32_t x, int32_t y)
{
    return self->x <= x && x < self->x + self->w
        && self->y <= y && y < self->y + self->h;
}

void jlos_composite_widget_init(jlos_composite_widget_t* self, jlos_widget_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b)
{
    jlos_widget_init(&self->base_widget, parent, x, y, w, h, r, g, b);
    self->base_widget.get_focus = (void (*)(jlos_widget_t*, jlos_widget_t*))jlos_composite_widget_get_focus;
    self->base_widget.draw = (void (*)(jlos_widget_t*, jlos_graphics_context_t*))jlos_composite_widget_draw;
    self->base_widget.on_mouse_down = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_composite_widget_on_mouse_down;
    self->base_widget.on_mouse_up = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_composite_widget_on_mouse_up;
    self->base_widget.mouse_move = (void (*)(jlos_widget_t*, int32_t, int32_t, int32_t, int32_t))jlos_composite_widget_mouse_move;
    self->base_widget.key_down = (void (*)(jlos_widget_t*, char))jlos_composite_widget_onkey_down;
    self->base_widget.on_key_up = (void (*)(jlos_widget_t*, char))jlos_composite_widget_onkey_up;
    self->focussed_child = NULL;
    self->num_children = 0;
}

void jlos_composite_widget_destroy(jlos_composite_widget_t* self)
{
    jlos_widget_destroy(&self->base_widget);
}

void jlos_composite_widget_get_focus(jlos_composite_widget_t* self, jlos_widget_t *widget)
{
    self->focussed_child = widget;
    if (self->base_widget.parent != NULL) {
        self->base_widget.parent->get_focus(self->base_widget.parent, (jlos_widget_t*)self);
    }
}

bool jlos_composite_widget_add_child(jlos_composite_widget_t* self, jlos_widget_t *child)
{
    if (self->num_children >= 100) {
        return false;
    }
    self->children[self->num_children++] = child;
    return true;
}

void jlos_composite_widget_draw(jlos_composite_widget_t* self, jlos_graphics_context_t *gc)
{
    jlos_widget_draw(&self->base_widget, gc);
    for (int i = self->num_children - 1; i >= 0; --i) {
        self->children[i]->draw(self->children[i], gc);
    }
}

void jlos_composite_widget_on_mouse_down(jlos_composite_widget_t* self, int32_t x, int32_t y, uint8_t button)
{
    for (int i = 0; i < self->num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], x - self->base_widget.x, y - self->base_widget.y)) {
            self->children[i]->on_mouse_down(self->children[i], x - self->base_widget.x, y - self->base_widget.y, button);
            break;
        }
    }
}

void jlos_composite_widget_on_mouse_up(jlos_composite_widget_t* self, int32_t x, int32_t y, uint8_t button)
{
    for (int i = 0; i < self->num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], x - self->base_widget.x, y - self->base_widget.y)) {
            self->children[i]->on_mouse_up(self->children[i], x - self->base_widget.x, y - self->base_widget.y, button);
            break;
        }
    }
}

void jlos_composite_widget_mouse_move(jlos_composite_widget_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
    int firstchild = -1;
    for (int i = 0; i < self->num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], oldx - self->base_widget.x, oldy - self->base_widget.y)) {
            self->children[i]->mouse_move(self->children[i], oldx - self->base_widget.x, oldy - self->base_widget.y, newx - self->base_widget.x, newy - self->base_widget.y);
            firstchild = i;
            break;
        }
    }

    for (int i = 0; i < self->num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], newx - self->base_widget.x, newy - self->base_widget.y)) {
            if (firstchild != i) {
                self->children[i]->mouse_move(self->children[i], oldx - self->base_widget.x, oldy - self->base_widget.y, newx - self->base_widget.x, newy - self->base_widget.y);
            }
            break;
        }
    }
}

void jlos_composite_widget_onkey_down(jlos_composite_widget_t* self, char str)
{
    if (self->focussed_child != NULL) {
        self->focussed_child->key_down(self->focussed_child, str);
    }
}

void jlos_composite_widget_onkey_up(jlos_composite_widget_t* self, char str)
{
    if (self->focussed_child != NULL) {
        self->focussed_child->on_key_up(self->focussed_child, str);
    }
}