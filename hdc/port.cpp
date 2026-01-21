#include <hdc/port.h>

namespace JLOS {
namespace Hdc {
port::port(uint16_t portnumber)
{
	this->m_portnumber = portnumber;
}

port::~port(){}

port8_bit::port8_bit(uint16_t portnumber):port(portnumber){}

port8_bit::~port8_bit(){}

void port8_bit::write(uint8_t data)
{
	__asm__ volatile("outb %0, %1" :: "a" (data), "nd" (m_portnumber));
}

uint8_t port8_bit::read()
{
    uint8_t result;
	__asm__ volatile("inb %1, %0" : "=a" (result) : "nd" (m_portnumber));
    return result;
}

port8_bit_slow::port8_bit_slow(uint16_t portnumber):port8_bit(portnumber){}

port8_bit_slow::~port8_bit_slow(){}

void port8_bit_slow::write(uint8_t data)
{
	__asm__ volatile("outb %0, %1\njmp 1f\n1: jmp 1f\n1:" :: "a" (data), "nd" (m_portnumber));
}

port16_bit::port16_bit(uint16_t portnumber):port(portnumber){}

port16_bit::~port16_bit(){}

void port16_bit::write(uint16_t data)
{
	__asm__ volatile("outw %0, %1" :: "a" (data), "nd" (m_portnumber));
}

uint16_t port16_bit::read()
{
	uint16_t result;
	__asm__ volatile("inw %1, %0" : "=a" (result) : "nd" (m_portnumber));
	return result;
}

port32_bit::port32_bit(uint16_t portnumber):port(portnumber){}

port32_bit::~port32_bit(){}

void port32_bit::write(uint32_t data)
{
	__asm__ volatile("outl %0, %1" :: "a" (data), "nd" (m_portnumber));
}

uint32_t port32_bit::read()
{
	uint32_t result;
	__asm__ volatile("inl %1, %0" : "=a" (result) : "nd" (m_portnumber));
	return result;
}
}
}
