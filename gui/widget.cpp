#include <gui/widget.h>

namespace JLOS {
namespace Gui {
Widget::Widget(Widget *parent, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r,
     uint8_t g, uint8_t b) : KeyboardEventHandler(), parent(parent), x(x), y(y), w(w),
     h(h), r(r), g(g), b(b), Focussable(true){}

Widget::~Widget(){}

void Widget::GetFocus(Widget *widget)
{
     if (parent != 0) {
          parent->GetFocus(widget);
     }
}

void Widget::ModelToScreen(int32_t &x, int32_t &y)
{
     if (parent != 0) {
          parent->ModelToScreen(x, y);
     }
     x += this->x;
     y += this->y;
}

void Widget::Draw(GraphicsContext *gc)
{
     int X = 0;
     int Y = 0;
     ModelToScreen(X, Y);
     gc->FillRectangle(X, Y, w, h, r, g, b);
}

void Widget::OnMouseDown(int32_t x, int32_t y, uint8_t button)
{
     if (Focussable) {
          GetFocus(this);
     }
}

void Widget::OnMouseUp(int32_t x, int32_t y, uint8_t button){}

void Widget::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy){}

bool Widget::ContainsCoordinate(int32_t x, int32_t y)
{
     return this->x <= x && x < this->x + this->w
          && this->y <= y && y < this->y + this->h;
}

CompositeWidget::CompositeWidget(Widget *parent, int32_t x, int32_t y, int32_t w, int32_t h,
          uint8_t r, uint8_t g, uint8_t b) : Widget(parent, x, y, w, h, r, g, b),
          focussedChild(0), numChildren(0){}

CompositeWidget::~CompositeWidget(){}

void CompositeWidget::GetFocus(Widget *widget)
{
     this->focussedChild = widget;
     if (parent != 0) {
          parent->GetFocus(this);
     }
}

bool CompositeWidget::AddChild(Widget *child)
{
     if (numChildren >=100) {
          return false;
     }
     children[numChildren++] = child;
     return true;
}

void CompositeWidget::Draw(GraphicsContext *gc)
{
     Widget::Draw(gc);
     for (int i = numChildren - 1; i >= 0; --i) {
          children[i]->Draw(gc);
     }
}

void CompositeWidget::OnMouseDown(int32_t x, int32_t y, uint8_t button)
{
     for (int i = 0; i < numChildren; ++i) {
          if(children[i]->ContainsCoordinate(x - this->x, y - this->y)) {
               children[i]->OnMouseDown(x - this->x, y - this->y, button);
               break;
          }
     }
}

void CompositeWidget::OnMouseUp(int32_t x, int32_t y, uint8_t button)
{
     for (int i = 0; i < numChildren; ++i) {
          if (children[i]->ContainsCoordinate(x - this->x, y - this->y)) {
               children[i]->OnMouseUp(x - this->x, y - this->y, button);
               break;
          }
     }
}

void CompositeWidget::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy)
{
     //todo,优化算法
     int firstchild = -1;
     for (int i = 0; i < numChildren; ++i) {
          if (children[i]->ContainsCoordinate(oldx - this->x, oldy - this->y)) {
               children[i]->OnMouseMove(oldx - this->x, oldy - this->y, newx - this->x,
                    newy - this->y);
               firstchild = 1;
               break;
          }
     }

     for (int i = 0; i < numChildren; ++i) {
          if (children[i]->ContainsCoordinate(newx - this->x, newy - this->y)) {
               if (firstchild != i) {
                    children[i]->OnMouseMove(oldx - this->x, oldy - this->y, newx - this->x,
                         newy - this->y);
               }
               break;
          }
     }
}

void CompositeWidget::OnkeyDown(char str)
{
     if (focussedChild != 0) {
          focussedChild->OnKeyDown(str);
     }
}

void CompositeWidget::OnkeyUp(char str)
{
     if (focussedChild != 0) {
          focussedChild->OnKeyUp(str);
     }
}
}     
}
