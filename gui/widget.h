#ifndef __GUI_WIDGET_H
#define __GUI_WIDGET_H

#include <common/types.h>
#include <common/graphics.h>
#include <drivers/keyboard.h>

namespace JLOS {
namespace Gui {
class widget : public Drivers::keyboard_event_handler {
protected:
     widget *parent;
     int32_t m_x;
     int32_t m_y;
     int32_t m_w;
     int32_t m_h;

     uint8_t m_r;
     uint8_t m_g;
     uint8_t m_b;

     bool m_focussable;

public:
     widget(widget *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h,
          uint8_t m_r, uint8_t m_g, uint8_t m_b);
     ~widget();

     virtual void get_focus(widget *widget);
     virtual void model_to_screen(int32_t &m_x, int32_t &m_y);
     virtual bool contains_coordinate(int32_t m_x, int32_t m_y);

     virtual void draw(graphics_context *gc);
     virtual void on_mouse_down(int32_t m_x, int32_t m_y, uint8_t button);
     virtual void on_mouse_up(int32_t m_x, int32_t m_y, uint8_t button);
     virtual void mouse_move(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);
};

class composite_widget : public widget {
private:
     widget *children[100];
     int m_num_children;
     widget *focussed_child;

public:
     composite_widget(widget *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h,
          uint8_t m_r, uint8_t m_g, uint8_t m_b);
     ~composite_widget();
     
     virtual void get_focus(widget *widget);
     virtual bool add_child(widget *child);

     virtual void draw(graphics_context *gc);
     virtual void on_mouse_down(int32_t m_x, int32_t m_y, uint8_t button);
     virtual void on_mouse_up(int32_t m_x, int32_t m_y, uint8_t button);
     virtual void mouse_move(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);

     virtual void onkey_down(char);
     virtual void onkey_up(char);
};
}
}

#endif
