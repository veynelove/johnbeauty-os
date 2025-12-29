#ifndef __JLOS_GUI_WINDOW_H
#define __JLOS_GUI_WINDOW_H

#include <gui/widget.h>
#include <drivers/mouse.h>

namespace JLOS {
namespace Gui {
class Window : public CompositeWidget {
private:
     bool Dragging;

public:
     Window(Widget *parent, int32_t x, int32_t y, int32_t w, int32_t h,
          uint8_t r, uint8_t g, uint8_t b);
     ~Window();

     void OnMouseDown(int32_t x, int32_t y, uint8_t button);
     void OnMouseUp(int32_t x, int32_t y, uint8_t button);
     void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);
};
}
}
#endif
