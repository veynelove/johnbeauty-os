/**
 * 把鼠标硬编码到桌面并非好事，暂时如此处理
 */
#ifndef __JLOS_GUI_DESKTOP_H
#define __JLOS_GUI_DESKTOP_H

#include <gui/widget.h>
#include <drivers/mouse.h>

namespace JLOS {
namespace Gui {
class desktop : public composite_widget, public Drivers::mouse_event_handler {
private:
     uint32_t m_mouse_x;
     uint32_t m_mouse_y;
public:
     desktop(int32_t m_w, int32_t m_h, uint8_t m_r, uint8_t m_g, uint8_t m_b);
     ~desktop();

     void draw(graphics_context *gc);
     void on_mouse_down(uint8_t button);
     void on_mouse_up(uint8_t button);
     void mouse_move(int32_t xoffset, int32_t yoffset);
};
}
}

#endif
