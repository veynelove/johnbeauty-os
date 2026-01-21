#ifndef __DRIVERS_MOUSE_H
#define __DRIVERS_MOUSE_H

#include <common/types.h>
#include <hdc/interrupts.h>
#include <hdc/port.h>
#include <drivers/driver.h>

namespace JLOS {
namespace Drivers {
class mouse_event_handler {
public:
    mouse_event_handler();

    virtual void on_activate();
    virtual void on_mouse_down(uint8_t button);
    virtual void on_mouse_up(uint8_t button);
    virtual void mouse_move(int32_t xoffset, int32_t yoffset);
};

class mouse_driver : public Hdc::interrupt_handler, public driver {
private:
    Hdc::port8_bit m_dataport;
    Hdc::port8_bit m_commandport;
    uint8_t buffer[3];
    uint8_t m_offset;
    uint8_t m_buttons;
    mouse_event_handler *handler;

public:
    mouse_driver(Hdc::interrupt_manager *manager, mouse_event_handler *handler);
    ~mouse_driver();
    virtual uint32_t handle_interrupt(uint32_t m_esp);
    virtual void activate();
};
}
}
#endif
