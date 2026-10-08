/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ARCG_X86_IO_H
#define _ARCG_X86_IO_H

#include <hal/io.h>

void jlos_port_io8_init(jlos_io8_t* self, uint16_t portnumber);
void jlos_port_io8_write(jlos_io8_t* self, uint8_t data);
uint8_t jlos_port_io8_read(jlos_io8_t* self);

void jlos_port_io8_slow_init(jlos_io8_slow_t* self, uint16_t portnumber);
void jlos_port_io8_slow_write(jlos_io8_slow_t* self, uint8_t data);
uint8_t jlos_port_io8_slow_read(jlos_io8_slow_t* self);

void jlos_port_io16_init(jlos_io16_t* self, uint16_t portnumber);
void jlos_port_io16_write(jlos_io16_t* self, uint16_t data);
uint16_t jlos_port_io16_read(jlos_io16_t* self);

void jlos_port_io32_init(jlos_io32_t* self, uint16_t portnumber);
void jlos_port_io32_write(jlos_io32_t* self, uint32_t data);
uint32_t jlos_port_io32_read(jlos_io32_t* self);

uint8_t  jlos_mmio_read8(const volatile void *addr);
uint16_t jlos_mmio_read16(const volatile void *addr);
uint32_t jlos_mmio_read32(const volatile void *addr);

void jlos_mmio_write8(volatile void *addr, uint8_t val);
void jlos_mmio_write16(volatile void *addr, uint16_t val);
void jlos_mmio_write32(volatile void *addr, uint32_t val);

#endif