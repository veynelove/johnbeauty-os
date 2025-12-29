#ifndef __DRIVERS_KEYBOARD_H
#define __DRIVERS_KEYBOARD_H

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

class KeyboardDriver : public Hdc::InterruptHandle, public Driver {
private:    
    Hdc::Port8Bit dataport;
    Hdc::Port8Bit commandport;
    KeyboardEventHandler *handler;

public:
    KeyboardDriver(Hdc::InterruptManager *manager, KeyboardEventHandler *handler);
    ~KeyboardDriver();
    virtual uint32_t HandleInterrupt(uint32_t esp);
    virtual void Activate();
};
}
}
#endif
