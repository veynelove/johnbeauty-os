#ifndef __JLOS_GUI_WINDOW_H
#define __JLOS_GUI_WINDOW_H

#include <gui/widget.h>
#include <drivers/mouse.h>

namespace JLOS {
namespace Gui {
class window : public composite_widget {
private:
     bool m_dragging;

public:
     window(widget *parent, int32_t m_x, int32_t m_y, int32_t m_w, int32_t m_h,
          uint8_t m_r, uint8_t m_g, uint8_t m_b);
     ~window();

     void on_mouse_down(int32_t m_x, int32_t m_y, uint8_t button);
     void on_mouse_up(int32_t m_x, int32_t m_y, uint8_t button);
     void mouse_move(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);
};
}
}
#endif
