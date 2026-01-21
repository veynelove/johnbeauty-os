#include <drivers/keyboard.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printf_hex(uint8_t);
}

namespace JLOS {
namespace Drivers {
keyboard_event_handler::keyboard_event_handler(){}

void keyboard_event_handler::key_down(char){}

void keyboard_event_handler::on_key_up(char){}

keyboard_driver::keyboard_driver(JLOS::Hdc::interrupt_manager *manager,
    keyboard_event_handler *handler)
:interrupt_handler(manager, 0x21), m_dataport(0x60), m_commandport(0x64)
{
    this->handler = handler;
}

keyboard_driver::~keyboard_driver(){}

void keyboard_driver::activate()
{
    while (m_commandport.read() & 0x1) {
        m_dataport.read();
    }
    m_commandport.write(0xAE); //activate interrupts
    m_commandport.write(0x20); //get current m_state
    uint8_t status = (m_dataport.read() | 1) & ~0x10;
    m_commandport.write(0x60); //set m_state
    m_dataport.write(status);

    m_dataport.write(0xF4);
}

uint32_t keyboard_driver::handle_interrupt(uint32_t m_esp)
{
    uint8_t key = m_dataport.read();
    if (handler == 0) return m_esp;
    static bool shift=false;
    switch(key) {
        case 0x29:if(shift) handler->key_down('~');else handler->key_down('`');break;
        case 0x02:if(shift) handler->key_down('!');else handler->key_down('1');break;
        case 0x03:if(shift) handler->key_down('@');else handler->key_down('2');break;
        case 0x04:if(shift) handler->key_down('#');else handler->key_down('3');break;
        case 0x05:if(shift) handler->key_down('$');else handler->key_down('4');break;
        case 0x06:if(shift) handler->key_down('%');else handler->key_down('5');break;
        case 0x07:if(shift) handler->key_down('^');else handler->key_down('6');break;
        case 0x08:if(shift) handler->key_down('&');else handler->key_down('7');break;
        case 0x09:if(shift) handler->key_down('*');else handler->key_down('8');break;
        case 0x0A:if(shift) handler->key_down('(');else handler->key_down('9');break;
        case 0x0B:if(shift) handler->key_down(')');else handler->key_down('0');break;
        case 0x0C:if(shift) handler->key_down('_');else handler->key_down('-');break;
        case 0x0D:if(shift) handler->key_down('+');else handler->key_down('=');break;

        case 0x1A:if(shift) handler->key_down('{');else handler->key_down('[');break;
        case 0x1B:if(shift) handler->key_down('}');else handler->key_down(']');break;
        case 0x2B:if(shift) handler->key_down('|');else handler->key_down('\\');break;
        case 0x27:if(shift) handler->key_down(':');else handler->key_down(';');break;
        case 0x28:if(shift) handler->key_down('"');else handler->key_down('\'');break;
        case 0x33:if(shift) handler->key_down('<');else handler->key_down(',');break;
        case 0x34:if(shift) handler->key_down('>');else handler->key_down('.');break;
        case 0x35:if(shift) handler->key_down('?');else handler->key_down('/');break;

        case 0x10:if(shift) handler->key_down('Q');else handler->key_down('q');break;
        case 0x11:if(shift) handler->key_down('W');else handler->key_down('m_w');break;
        case 0x12:if(shift) handler->key_down('E');else handler->key_down('e');break;
        case 0x13:if(shift) handler->key_down('R');else handler->key_down('m_r');break;
        case 0x14:if(shift) handler->key_down('T');else handler->key_down('t');break;
        case 0x15:if(shift) handler->key_down('Y');else handler->key_down('m_y');break;
        case 0x16:if(shift) handler->key_down('U');else handler->key_down('u');break;
        case 0x17:if(shift) handler->key_down('I');else handler->key_down('i');break;
        case 0x18:if(shift) handler->key_down('O');else handler->key_down('o');break;
        case 0x19:if(shift) handler->key_down('P');else handler->key_down('p');break;
        case 0x1E:if(shift) handler->key_down('A');else handler->key_down('a');break;
        case 0x1F:if(shift) handler->key_down('S');else handler->key_down('s');break;
        case 0x20:if(shift) handler->key_down('D');else handler->key_down('d');break;
        case 0x21:if(shift) handler->key_down('F');else handler->key_down('f');break;
        case 0x22:if(shift) handler->key_down('G');else handler->key_down('m_g');break;
        case 0x23:if(shift) handler->key_down('H');else handler->key_down('m_h');break;
        case 0x24:if(shift) handler->key_down('J');else handler->key_down('j');break;
        case 0x25:if(shift) handler->key_down('K');else handler->key_down('k');break;
        case 0x26:if(shift) handler->key_down('L');else handler->key_down('l');break;
        case 0x2C:if(shift) handler->key_down('Z');else handler->key_down('z');break;
        case 0x2D:if(shift) handler->key_down('X');else handler->key_down('m_x');break;
        case 0x2E:if(shift) handler->key_down('C');else handler->key_down('c');break;
        case 0x2F:if(shift) handler->key_down('V');else handler->key_down('v');break;
        case 0x30:if(shift) handler->key_down('B');else handler->key_down('m_b');break;
        case 0x31:if(shift) handler->key_down('N');else handler->key_down('n');break;
        case 0x32:if(shift) handler->key_down('M');else handler->key_down('m');break;

        case 0x1C:handler->key_down('\n');break;
        case 0x39:handler->key_down(' ');break;
        
        case 0x2A:case 0x36:shift=true;break;
        case 0xAA:case 0xB6:shift=false;break;
        case 0x3A: //capslock
            break;
        
        default:
            if(key < 0x80) {
                Kernel::printf("KEYBOARD 0x");
                Kernel::printf_hex(key);
            }
            break;
        
    }
    return m_esp;
}
}
}
