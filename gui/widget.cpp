#include <gui/widget.h>

namespace JLOS {
namespace Gui {
widget::widget(widget *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h, uint8_t m_r,
     uint8_t m_g, uint8_t m_b) : keyboard_event_handler(), parent(parent), m_x(m_x), m_y(m_y), m_w(m_w),
     m_h(m_h), m_r(m_r), m_g(m_g), m_b(m_b), m_focussable(true){}

widget::~widget(){}

void widget::get_focus(widget *widget)
{
     if (parent != 0) {
          parent->get_focus(widget);
     }
}

void widget::model_to_screen(int32_t &m_x, int32_t &m_y)
{
     if (parent != 0) {
          parent->model_to_screen(m_x, m_y);
     }
     m_x += this->m_x;
     m_y += this->m_y;
}

void widget::draw(graphics_context *gc)
{
     int X = 0;
     int Y = 0;
     model_to_screen(X, Y);
     gc->fill_rectangle(X, Y, m_w, m_h, m_r, m_g, m_b);
}

void widget::on_mouse_down(int32_t m_x, int32_t m_y, uint8_t button)
{
     if (m_focussable) {
          get_focus(this);
     }
}

void widget::on_mouse_up(int32_t m_x, int32_t m_y, uint8_t button){}

void widget::mouse_move(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy){}

bool widget::contains_coordinate(int32_t m_x, int32_t m_y)
{
     return this->m_x <= m_x && m_x < this->m_x + this->m_w
          && this->m_y <= m_y && m_y < this->m_y + this->m_h;
}

composite_widget::composite_widget(widget *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h,
          uint8_t m_r, uint8_t m_g, uint8_t m_b) : widget(parent, m_x, m_y, m_w, m_h, m_r, m_g, m_b),
          focussed_child(0), m_num_children(0){}

composite_widget::~composite_widget(){}

void composite_widget::get_focus(widget *widget)
{
     this->focussed_child = widget;
     if (parent != 0) {
          parent->get_focus(this);
     }
}

bool composite_widget::add_child(widget *child)
{
     if (m_num_children >=100) {
          return false;
     }
     children[m_num_children++] = child;
     return true;
}

void composite_widget::draw(graphics_context *gc)
{
     widget::draw(gc);
     for (int i = m_num_children - 1; i >= 0; --i) {
          children[i]->draw(gc);
     }
}

void composite_widget::on_mouse_down(int32_t m_x, int32_t m_y, uint8_t button)
{
     for (int i = 0; i < m_num_children; ++i) {
          if(children[i]->contains_coordinate(m_x - this->m_x, m_y - this->m_y)) {
               children[i]->on_mouse_down(m_x - this->m_x, m_y - this->m_y, button);
               break;
          }
     }
}

void composite_widget::on_mouse_up(int32_t m_x, int32_t m_y, uint8_t button)
{
     for (int i = 0; i < m_num_children; ++i) {
          if (children[i]->contains_coordinate(m_x - this->m_x, m_y - this->m_y)) {
               children[i]->on_mouse_up(m_x - this->m_x, m_y - this->m_y, button);
               break;
          }
     }
}

void composite_widget::mouse_move(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
     //todo,优化算法
     int firstchild = -1;
     for (int i = 0; i < m_num_children; ++i) {
          if (children[i]->contains_coordinate(oldx - this->m_x, oldy - this->m_y)) {
               children[i]->mouse_move(oldx - this->m_x, oldy - this->m_y, newx - this->m_x,
                    newy - this->m_y);
               firstchild = 1;
               break;
          }
     }

     for (int i = 0; i < m_num_children; ++i) {
          if (children[i]->contains_coordinate(newx - this->m_x, newy - this->m_y)) {
               if (firstchild != i) {
                    children[i]->mouse_move(oldx - this->m_x, oldy - this->m_y, newx - this->m_x,
                         newy - this->m_y);
               }
               break;
          }
     }
}

void composite_widget::onkey_down(char str)
{
     if (focussed_child != 0) {
          focussed_child->key_down(str);
     }
}

void composite_widget::onkey_up(char str)
{
     if (focussed_child != 0) {
          focussed_child->on_key_up(str);
     }
}
}     
}
