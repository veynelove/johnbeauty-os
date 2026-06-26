#include <gui/widget.h>

void jlos_widget_init(jlos_widget_t* self, jlos_widget_t *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h, uint8_t m_r, uint8_t m_g, uint8_t m_b)
{
    self->parent = parent;
    self->m_x = m_x;
    self->m_y = m_y;
    self->m_w = m_w;
    self->m_h = m_h;
    self->m_r = m_r;
    self->m_g = m_g;
    self->m_b = m_b;
    self->m_focussable = true;
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

void jlos_widget_model_to_screen(jlos_widget_t* self, int32_t *m_x, int32_t *m_y)
{
    if (self->parent != NULL) {
        self->parent->model_to_screen(self->parent, m_x, m_y);
    }
    *m_x += self->m_x;
    *m_y += self->m_y;
}

void jlos_widget_draw(jlos_widget_t* self, jlos_graphics_context_t *gc)
{
    int X = 0;
    int Y = 0;
    self->model_to_screen(self, &X, &Y);
    jlos_graphics_context_fill_rectangle(gc, X, Y, self->m_w, self->m_h, self->m_r, self->m_g, self->m_b);
}

void jlos_widget_on_mouse_down(jlos_widget_t* self, int32_t m_x, int32_t m_y, uint8_t button)
{
    if (self->m_focussable) {
        self->get_focus(self, self);
    }
}

void jlos_widget_on_mouse_up(jlos_widget_t* self, int32_t m_x, int32_t m_y, uint8_t button)
{
}

void jlos_widget_mouse_move(jlos_widget_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
}

bool jlos_widget_contains_coordinate(jlos_widget_t* self, int32_t m_x, int32_t m_y)
{
    return self->m_x <= m_x && m_x < self->m_x + self->m_w
        && self->m_y <= m_y && m_y < self->m_y + self->m_h;
}

void jlos_composite_widget_init(jlos_composite_widget_t* self, jlos_widget_t *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h, uint8_t m_r, uint8_t m_g, uint8_t m_b)
{
    jlos_widget_init(&self->base_widget, parent, m_x, m_y, m_w, m_h, m_r, m_g, m_b);
    self->base_widget.get_focus = (void (*)(jlos_widget_t*, jlos_widget_t*))jlos_composite_widget_get_focus;
    self->base_widget.draw = (void (*)(jlos_widget_t*, jlos_graphics_context_t*))jlos_composite_widget_draw;
    self->base_widget.on_mouse_down = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_composite_widget_on_mouse_down;
    self->base_widget.on_mouse_up = (void (*)(jlos_widget_t*, int32_t, int32_t, uint8_t))jlos_composite_widget_on_mouse_up;
    self->base_widget.mouse_move = (void (*)(jlos_widget_t*, int32_t, int32_t, int32_t, int32_t))jlos_composite_widget_mouse_move;
    self->base_widget.key_down = (void (*)(jlos_widget_t*, char))jlos_composite_widget_onkey_down;
    self->base_widget.on_key_up = (void (*)(jlos_widget_t*, char))jlos_composite_widget_onkey_up;
    self->focussed_child = NULL;
    self->m_num_children = 0;
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
    if (self->m_num_children >= 100) {
        return false;
    }
    self->children[self->m_num_children++] = child;
    return true;
}

void jlos_composite_widget_draw(jlos_composite_widget_t* self, jlos_graphics_context_t *gc)
{
    jlos_widget_draw(&self->base_widget, gc);
    for (int i = self->m_num_children - 1; i >= 0; --i) {
        self->children[i]->draw(self->children[i], gc);
    }
}

void jlos_composite_widget_on_mouse_down(jlos_composite_widget_t* self, int32_t m_x, int32_t m_y, uint8_t button)
{
    for (int i = 0; i < self->m_num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], m_x - self->base_widget.m_x, m_y - self->base_widget.m_y)) {
            self->children[i]->on_mouse_down(self->children[i], m_x - self->base_widget.m_x, m_y - self->base_widget.m_y, button);
            break;
        }
    }
}

void jlos_composite_widget_on_mouse_up(jlos_composite_widget_t* self, int32_t m_x, int32_t m_y, uint8_t button)
{
    for (int i = 0; i < self->m_num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], m_x - self->base_widget.m_x, m_y - self->base_widget.m_y)) {
            self->children[i]->on_mouse_up(self->children[i], m_x - self->base_widget.m_x, m_y - self->base_widget.m_y, button);
            break;
        }
    }
}

void jlos_composite_widget_mouse_move(jlos_composite_widget_t* self, int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
    int firstchild = -1;
    for (int i = 0; i < self->m_num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], oldx - self->base_widget.m_x, oldy - self->base_widget.m_y)) {
            self->children[i]->mouse_move(self->children[i], oldx - self->base_widget.m_x, oldy - self->base_widget.m_y, newx - self->base_widget.m_x, newy - self->base_widget.m_y);
            firstchild = i;
            break;
        }
    }

    for (int i = 0; i < self->m_num_children; ++i) {
        if (self->children[i]->contains_coordinate(self->children[i], newx - self->base_widget.m_x, newy - self->base_widget.m_y)) {
            if (firstchild != i) {
                self->children[i]->mouse_move(self->children[i], oldx - self->base_widget.m_x, oldy - self->base_widget.m_y, newx - self->base_widget.m_x, newy - self->base_widget.m_y);
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