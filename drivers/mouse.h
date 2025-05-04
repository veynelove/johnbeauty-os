#ifndef __MOUSE_H
#define __MOUSE_H

#include <common/types.h>
#include <hdc/interrupts.h>
#include <hdc/port.h>
#include <drivers/driver.h>

namespace JLOS {
namespace Drivers {
class MouseEventHandler {
public:
    MouseEventHandler();

    virtual void OnActivate();
    virtual void OnMouseDown(uint8_t button);
    virtual void OnMouseUp(uint8_t button);
    virtual void OnMouseMove(int32_t xoffset, int32_t yoffset);
};

class MouseDriver : public JLOS::Hdc::InterruptHandle, public Driver {
public:
    MouseDriver(JLOS::Hdc::InterruptManager *manager, MouseEventHandler *handler);
    ~MouseDriver();
    virtual uint32_t HandleInterrupt(uint32_t esp);
    virtual void Activate();
private:
    JLOS::Hdc::Port8Bit dataport;
    JLOS::Hdc::Port8Bit commandport;

    uint8_t buffer[3];
    uint8_t offset;
    uint8_t buttons;

    MouseEventHandler *handler;
};
}
}
#endif
