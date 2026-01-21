#include <gui/desktop.h>
namespace JLOS {
namespace Gui {
desktop::desktop(int32_t m_w, int32_t m_h, uint8_t m_r, uint8_t m_g, uint8_t m_b)
: composite_widget(0, 0, 0, m_w, m_h, m_r, m_g, m_b), mouse_event_handler(), m_mouse_x(m_w/2), m_mouse_y(m_h/2){}

desktop::~desktop(){}

void desktop::draw(graphics_context *gc)
{
     composite_widget::draw(gc);
     for (int i = 0; i < 4; i++) {
          gc->put_pixel(m_mouse_x-i, m_mouse_y, 0xFF, 0xFF, 0xFF);
          gc->put_pixel(m_mouse_x+i, m_mouse_y, 0xFF, 0xFF, 0xFF);
          gc->put_pixel(m_mouse_x, m_mouse_y-i, 0xFF, 0xFF, 0xFF);
          gc->put_pixel(m_mouse_x, m_mouse_y+i, 0xFF, 0xFF, 0xFF);
     }
}

void desktop::on_mouse_down(uint8_t button)
{
     composite_widget::on_mouse_down(m_mouse_x, m_mouse_y, button);
}

void desktop::on_mouse_up(uint8_t button)
{
     composite_widget::on_mouse_up(m_mouse_x, m_mouse_y, button);
}

void desktop::mouse_move(int32_t xoffset, int32_t yoffset)
{
     xoffset /= 4;
     yoffset /= 4;

     int32_t new_mouse_x = m_mouse_x + xoffset;
     if (new_mouse_x < 0) new_mouse_x = 0;
     if (new_mouse_x >= m_w) new_mouse_x = m_w - 1;

     int32_t new_mouse_y = m_mouse_y + yoffset;
     if (new_mouse_y < 0) new_mouse_y = 0;
     if (new_mouse_y >= m_h) new_mouse_y = m_h - 1;
     composite_widget::mouse_move(m_mouse_x, m_mouse_y, new_mouse_x, new_mouse_y);
     m_mouse_x = new_mouse_x;
     m_mouse_y = new_mouse_y;
}
}
}
