/**
 * 把鼠标硬编码到桌面并非好事，暂时如此处理
 */
#ifndef __JLOS_GUI_DESKTOP_H
#define __JLOS_GUI_DESKTOP_H

#include <gui/widget.h>
#include <drivers/mouse.h>

namespace JLOS {
namespace Gui {
class Desktop : public CompositeWidget, public Drivers::MouseEventHandler {
private:
     uint32_t MouseX;
     uint32_t MouseY;
public:
     Desktop(int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b);
     ~Desktop();

     void Draw(GraphicsContext *gc);
     void OnMouseDown(uint8_t button);
     void OnMouseUp(uint8_t button);
     void OnMouseMove(int32_t xoffset, int32_t yoffset);
};
}
}

#endif
