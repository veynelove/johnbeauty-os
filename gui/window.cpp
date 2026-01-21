#include <gui/window.h>

namespace JLOS {
namespace Gui {
window::window(widget *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h, uint8_t m_r,
     uint8_t m_g, uint8_t m_b) : composite_widget(parent, m_x, m_y, m_w, m_h, m_r ,m_g ,m_b), m_dragging(false){}

window::~window(){}

void window::on_mouse_down(int32_t m_x, int32_t m_y, uint8_t button)
{
     m_dragging = (button == 1);
     composite_widget::on_mouse_down(m_x, m_y, button);
}

void window::on_mouse_up(int32_t m_x, int32_t m_y, uint8_t button)
{
     m_dragging = false;
     composite_widget::on_mouse_up(m_x, m_y, button);
}

void window::mouse_move(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
     if (m_dragging) {
          this->m_x += newx - oldx;
          this->m_y += newy -oldy;
     }
     composite_widget::mouse_move(oldx, oldy, newx, newy);
}
}
}