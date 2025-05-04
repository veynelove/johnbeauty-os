#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <common/types.h>
#include <hdc/interrupts.h>
#include <hdc/port.h>
#include <drivers/driver.h>

namespace JLOS {
namespace Drivers {
class KeyboardEventHandler {
public:
    KeyboardEventHandler();

    virtual void OnKeyDown(char);
    virtual void OnKeyUp(char);
};

class KeyboardDriver : public JLOS::Hdc::InterruptHandle, public Driver {
private:    
    JLOS::Hdc::Port8Bit dataport;
    JLOS::Hdc::Port8Bit commandport;

    KeyboardEventHandler *handler;
public:
    KeyboardDriver(JLOS::Hdc::InterruptManager *manager, KeyboardEventHandler *handler);
    ~KeyboardDriver();
    virtual uint32_t HandleInterrupt(uint32_t esp);
    virtual void Activate();
};
}
}
#endif
