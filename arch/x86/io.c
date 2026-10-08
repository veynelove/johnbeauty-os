/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/io.h>
#include <hal/hal.h>

void jlos_port_io8_init(jlos_io8_t* self, uint16_t portnumber)
{
    self->portnumber = portnumber;
}

void jlos_port_io8_write(jlos_io8_t* self, uint8_t data)
{
    __asm__ volatile("outb %0, %1" :: "a" (data), "nd" (self->portnumber));
}

uint8_t jlos_port_io8_read(jlos_io8_t* self)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a" (result) : "nd" (self->portnumber));
    return result;
}

void jlos_port_io8_slow_init(jlos_io8_slow_t* self, uint16_t portnumber)
{
    self->portnumber = portnumber;
}

void jlos_port_io8_slow_write(jlos_io8_slow_t* self, uint8_t data)
{
    __asm__ volatile("outb %0, %1\njmp 1f\n1: jmp 1f\n1:" :: "a" (data), "nd" (self->portnumber));
}

uint8_t jlos_port_io8_slow_read(jlos_io8_slow_t* self)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a" (result) : "nd" (self->portnumber));
    return result;
}

void jlos_port_io16_init(jlos_io16_t* self, uint16_t portnumber)
{
    self->portnumber = portnumber;
}

void jlos_port_io16_write(jlos_io16_t* self, uint16_t data)
{
    __asm__ volatile("outw %0, %1" :: "a" (data), "nd" (self->portnumber));
}

uint16_t jlos_port_io16_read(jlos_io16_t* self)
{
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a" (result) : "nd" (self->portnumber));
    return result;
}

void jlos_port_io32_init(jlos_io32_t* self, uint16_t portnumber)
{
    self->portnumber = portnumber;
}

void jlos_port_io32_write(jlos_io32_t* self, uint32_t data)
{
    __asm__ volatile("outl %0, %1" :: "a" (data), "nd" (self->portnumber));
}

uint32_t jlos_port_io32_read(jlos_io32_t* self)
{
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a" (result) : "nd" (self->portnumber));
    return result;
}

uint8_t jlos_mmio_read8(const volatile void *addr)
{
    return *(const volatile uint8_t *)addr;
}

uint16_t jlos_mmio_read16(const volatile void *addr)
{
    return *(const volatile uint16_t *)addr;
}

uint32_t jlos_mmio_read32(const volatile void *addr)
{
    return *(const volatile uint32_t *)addr;
}

void jlos_mmio_write8(volatile void *addr, uint8_t val)
{
    *(volatile uint8_t *)addr = val;
}

void jlos_mmio_write16(volatile void *addr, uint16_t val)
{
    *(volatile uint16_t *)addr = val;
}

void jlos_mmio_write32(volatile void *addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

const jlos_hal_io_ops_t jlos_hal_x86_io_ops = {
    .init_io8         = jlos_port_io8_init,
    .write_io8        = jlos_port_io8_write,
    .read_io8         = jlos_port_io8_read,
    .init_io8_slow    = jlos_port_io8_slow_init,
    .write_io8_slow   = jlos_port_io8_slow_write,
    .read_io8_slow    = jlos_port_io8_slow_read,
    .init_io16        = jlos_port_io16_init,
    .write_io16       = jlos_port_io16_write,
    .read_io16        = jlos_port_io16_read,
    .init_io32        = jlos_port_io32_init,
    .write_io32       = jlos_port_io32_write,
    .read_io32        = jlos_port_io32_read,
    .read_mmio8       = jlos_mmio_read8,
    .read_mmio16      = jlos_mmio_read16,
    .read_mmio32      = jlos_mmio_read32,
    .write_mmio8      = jlos_mmio_write8,
    .write_mmio16     = jlos_mmio_write16,
    .write_mmio32     = jlos_mmio_write32,
};

const jlos_hal_io_ops_t *jlos_hal_io_ops = &jlos_hal_x86_io_ops;
