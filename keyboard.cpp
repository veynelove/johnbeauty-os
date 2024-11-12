#include "keyboard.h"

void printf(const char *);

KeyboardDriver::KeyboardDriver(InterruptManager *manager)
:InterruptHandle(0x21, manager), dataport(0x60), commandport(0x64)
{
    while(commandport.Read() & 0x1)
        dataport.Read();
    commandport.Write(0xAE); //activate interrupts
    commandport.Write(0x20); //get current state
    uint8_t status = (dataport.Read() | 1) & ~0x10;
    commandport.Write(0x60); //set state
    dataport.Write(status);

    dataport.Write(0xF4);
}

KeyboardDriver::~KeyboardDriver()
{

}

uint32_t KeyboardDriver::HandleInterrupt(uint32_t esp)
{
    uint8_t key = dataport.Read();
    if(key < 0x80) {
        switch(key)
        {
            case 0xFA: break;
            case 0x45: case 0xC5: break;
            default:
                char *foo = "0X00 ";
                char *hex = "0123456789ABCDEF";
                foo[2] = hex[(key>>4) & 0x0F];
                foo[3] = hex[key & 0x0F];
                printf(foo);
                break;
        }
    }
    return esp;
}
