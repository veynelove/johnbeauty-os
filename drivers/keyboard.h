#ifndef __DRIVERS_KEYBOARD_H
#define __DRIVERS_KEYBOARD_H

#include <common/types.h>
#include <hdc/interrupts.h>
#include <hdc/port.h>
#include <drivers/driver.h>

namespace JLOS {
namespace Drivers {
class keyboard_event_handler {
public:
    keyboard_event_handler();

    virtual void key_down(char);
    virtual void on_key_up(char);
};

class keyboard_driver : public Hdc::interrupt_handler, public driver {
private:    
    Hdc::port8_bit m_dataport;
    Hdc::port8_bit m_commandport;
    keyboard_event_handler *handler;

public:
    keyboard_driver(Hdc::interrupt_manager *manager, keyboard_event_handler *handler);
    ~keyboard_driver();
    virtual uint32_t handle_interrupt(uint32_t m_esp);
    virtual void activate();
};
}
}
#endif
