#include <arch/x86/port.h>

void jlos_port8_bit_init(jlos_port8_bit_t* self, uint16_t portnumber)
{
    self->m_portnumber = portnumber;
}

void jlos_port8_bit_write(jlos_port8_bit_t* self, uint8_t data)
{
    __asm__ volatile("outb %0, %1" :: "a" (data), "nd" (self->m_portnumber));
}

uint8_t jlos_port8_bit_read(jlos_port8_bit_t* self)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a" (result) : "nd" (self->m_portnumber));
    return result;
}

void jlos_port8_bit_slow_init(jlos_port8_bit_slow_t* self, uint16_t portnumber)
{
    self->m_portnumber = portnumber;
}

void jlos_port8_bit_slow_write(jlos_port8_bit_slow_t* self, uint8_t data)
{
    __asm__ volatile("outb %0, %1\njmp 1f\n1: jmp 1f\n1:" :: "a" (data), "nd" (self->m_portnumber));
}

uint8_t jlos_port8_bit_slow_read(jlos_port8_bit_slow_t* self)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a" (result) : "nd" (self->m_portnumber));
    return result;
}

void jlos_port16_bit_init(jlos_port16_bit_t* self, uint16_t portnumber)
{
    self->m_portnumber = portnumber;
}

void jlos_port16_bit_write(jlos_port16_bit_t* self, uint16_t data)
{
    __asm__ volatile("outw %0, %1" :: "a" (data), "nd" (self->m_portnumber));
}

uint16_t jlos_port16_bit_read(jlos_port16_bit_t* self)
{
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a" (result) : "nd" (self->m_portnumber));
    return result;
}

void jlos_port32_bit_init(jlos_port32_bit_t* self, uint16_t portnumber)
{
    self->m_portnumber = portnumber;
}

void jlos_port32_bit_write(jlos_port32_bit_t* self, uint32_t data)
{
    __asm__ volatile("outl %0, %1" :: "a" (data), "nd" (self->m_portnumber));
}

uint32_t jlos_port32_bit_read(jlos_port32_bit_t* self)
{
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a" (result) : "nd" (self->m_portnumber));
    return result;
}