#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"
#include "interrupts.h"
#include "port.h"
#include "driver.h"

class KeyboardEventHandler {
public:
    KeyboardEventHandler();

    virtual void OnKeyDown(char);
    virtual void OnKeyUp(char);
};

class KeyboardDriver : public InterruptHandle, public Driver {
private:    
    Port8Bit dataport;
    Port8Bit commandport;

    KeyboardEventHandler *handler;
public:
    KeyboardDriver(InterruptManager *manager, KeyboardEventHandler *handler);
    ~KeyboardDriver();
    virtual uint32_t HandleInterrupt(uint32_t esp);
    virtual void Activate();
};
#endif
