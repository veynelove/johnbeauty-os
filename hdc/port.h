#ifndef __HDC_PORT_H
#define __HDC_PORT_H

#include <common/types.h>

namespace JLOS {
namespace Hdc {
class port {
protected:
	uint16_t m_portnumber;
	port(uint16_t portnumber_);
	~port();
};

class port8_bit: public port {
public:
	port8_bit(uint16_t portnumber_);
	~port8_bit();
	virtual void write(uint8_t data);
	virtual uint8_t read();
};

class port8_bit_slow: public port8_bit {
public:
	port8_bit_slow(uint16_t portnumber_);
	~port8_bit_slow();
	virtual void write(uint8_t data);
};

class port16_bit: public port {
public:
	port16_bit(uint16_t portnumber_);
	~port16_bit();
	virtual void write(uint16_t data);
	virtual uint16_t read();
};

class port32_bit: public port {
public:
	port32_bit(uint16_t portnumber_);
	~port32_bit();
	virtual void write(uint32_t data);
	virtual uint32_t read();
};
}
}	
#endif