/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_HAL_IO_H
#define _JLOS_HAL_IO_H

#include <common/types.h>

typedef struct {
    uint16_t portnumber;
} jlos_io8_t;

typedef struct {
    uint16_t portnumber;
} jlos_io8_slow_t;

typedef struct {
    uint16_t portnumber;
} jlos_io16_t;

typedef struct {
    uint16_t portnumber;
} jlos_io32_t;

void jlos_io8_init(jlos_io8_t *self, uint16_t port);
void jlos_io8_write(jlos_io8_t *self, uint8_t val);
uint8_t jlos_io8_read(jlos_io8_t *self);

void jlos_io8_slow_init(jlos_io8_slow_t *self, uint16_t port);
void jlos_io8_slow_write(jlos_io8_slow_t *self, uint8_t val);
uint8_t jlos_io8_slow_read(jlos_io8_slow_t *self);

void jlos_io16_init(jlos_io16_t *self, uint16_t port);
void jlos_io16_write(jlos_io16_t *self, uint16_t val);
uint16_t jlos_io16_read(jlos_io16_t *self);

void jlos_io32_init(jlos_io32_t *self, uint16_t port);
void jlos_io32_write(jlos_io32_t *self, uint32_t val);
uint32_t jlos_io32_read(jlos_io32_t *self);

uint8_t jlos_io_mmio_read8(const volatile void *addr);
uint16_t jlos_io_mmio_read16(const volatile void *addr);
uint32_t jlos_io_mmio_read32(const volatile void *addr);

void jlos_io_mmio_write8(volatile void *addr, uint8_t val);
void jlos_io_mmio_write16(volatile void *addr, uint16_t val);
void jlos_io_mmio_write32(volatile void *addr, uint32_t val);

#endif
