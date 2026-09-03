#ifndef _HDC_PORT_H
#define _HDC_PORT_H

#include <common/types.h>

typedef struct {
    uint16_t portnumber;
} jlos_port8_bit_t;

typedef struct {
    uint16_t portnumber;
} jlos_port8_bit_slow_t;

typedef struct {
    uint16_t portnumber;
} jlos_port16_bit_t;

typedef struct {
    uint16_t portnumber;
} jlos_port32_bit_t;

void jlos_port8_bit_init(jlos_port8_bit_t* self, uint16_t portnumber);
void jlos_port8_bit_write(jlos_port8_bit_t* self, uint8_t data);
uint8_t jlos_port8_bit_read(jlos_port8_bit_t* self);

void jlos_port8_bit_slow_init(jlos_port8_bit_slow_t* self, uint16_t portnumber);
void jlos_port8_bit_slow_write(jlos_port8_bit_slow_t* self, uint8_t data);
uint8_t jlos_port8_bit_slow_read(jlos_port8_bit_slow_t* self);

void jlos_port16_bit_init(jlos_port16_bit_t* self, uint16_t portnumber);
void jlos_port16_bit_write(jlos_port16_bit_t* self, uint16_t data);
uint16_t jlos_port16_bit_read(jlos_port16_bit_t* self);

void jlos_port32_bit_init(jlos_port32_bit_t* self, uint16_t portnumber);
void jlos_port32_bit_write(jlos_port32_bit_t* self, uint32_t data);
uint32_t jlos_port32_bit_read(jlos_port32_bit_t* self);

#endif