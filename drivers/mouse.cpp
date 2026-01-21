#include <drivers/mouse.h>

namespace JLOS::Kernel {
void printf(const char *);
}

namespace JLOS {
namespace Drivers {
mouse_event_handler::mouse_event_handler(){}

void mouse_event_handler::on_activate(){}

void mouse_event_handler::on_mouse_down(uint8_t button){}

void mouse_event_handler::on_mouse_up(uint8_t button){}

void mouse_event_handler::mouse_move(int32_t xoffset, int32_t yoffset){}

mouse_driver::mouse_driver(JLOS::Hdc::interrupt_manager *manager, mouse_event_handler *handler)
:interrupt_handler(manager, 0x2C), m_dataport(0x60), m_commandport(0x64)
{
    this->handler = handler;
}

mouse_driver::~mouse_driver(){}

void mouse_driver::activate()
{
    m_offset=0;
    m_buttons=0;
    
    m_commandport.write(0xA8);
    m_commandport.write(0x20);
    uint8_t status =m_dataport.read() | 2;
    m_commandport.write(0x60);
    m_dataport.write(status);

    m_commandport.write(0xD4);
    m_dataport.write(0xF4);
    m_dataport.read();
}

uint32_t mouse_driver::handle_interrupt(uint32_t m_esp)
{
    uint8_t status = m_commandport.read();
    if (!(status & 0x20)) {
        return m_esp;
    }
    buffer[m_offset] = m_dataport.read();
    if (handler == 0) {
        return m_esp;
    }
    if (m_offset == 0 && !(buffer[0] & 0x08)) {
        return m_esp;
    } 
    m_offset = (m_offset + 1) % 3;

    if (m_offset == 0) {
        if(buffer[1] != 0 || buffer[2] != 0) {
            handler->mouse_move((int8_t)buffer[1], -((int8_t)buffer[2]));
        }
        for (uint8_t i = 0; i < 3; i++) {
            if((buffer[0] & (0x1 << i)) != (m_buttons & (0x1 << i))) {
                if(m_buttons & (0x1 << i)) {
                    handler->on_mouse_up(i+1);
                }
                else {    
                    handler->on_mouse_down(i+1);
                }
            }
        }
        m_buttons = buffer[0];
    }
    return m_esp;
}
}
}
