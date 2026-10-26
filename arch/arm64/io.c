/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/io.h>
#include <hal/hal.h>
#include <common/types.h>


#define JLOS_MMIO_BARRIER_DMB_SY  "dmb sy"

static uint8_t jlos_arm64_mmio_read8(const volatile void *addr)
{
    uint8_t val;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    val = *(const volatile uint8_t *)addr;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    return val;
}

static uint16_t jlos_arm64_mmio_read16(const volatile void *addr)
{
    uint16_t val;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    val = *(const volatile uint16_t *)addr;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    return val;
}

static uint32_t jlos_arm64_mmio_read32(const volatile void *addr)
{
    uint32_t val;

    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    val = *(const volatile uint32_t *)addr;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    return val;
}

static void jlos_arm64_mmio_write8(volatile void *addr, uint8_t val)
{
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    *(volatile uint8_t *)addr = val;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
}

static void jlos_arm64_mmio_write16(volatile void *addr, uint16_t val)
{
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    *(volatile uint16_t *)addr = val;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
}

static void jlos_arm64_mmio_write32(volatile void *addr, uint32_t val)
{
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
    *(volatile uint32_t *)addr = val;
    __asm__ volatile (JLOS_MMIO_BARRIER_DMB_SY ::: "memory");
}

static void jlos_arm64_port_init(jlos_io8_t *self, uint16_t port)
{
    self->portnumber = port;
}

static void jlos_arm64_port_init_slow(jlos_io8_slow_t *self, uint16_t port)
{
    self->portnumber = port;
}

static void jlos_arm64_port_init16(jlos_io16_t *self, uint16_t port)
{
    self->portnumber = port;
}

static void jlos_arm64_port_init32(jlos_io32_t *self, uint16_t port)
{
    self->portnumber = port;
}

static void jlos_arm64_port_write8(jlos_io8_t *self, uint8_t val)
{
    (void)self; (void)val;
}

static uint8_t jlos_arm64_port_read8(jlos_io8_t *self)
{
    (void)self;
    return 0;
}

static void jlos_arm64_port_write8_slow(jlos_io8_slow_t *self, uint8_t val)
{
    (void)self; (void)val;
}

static uint8_t jlos_arm64_port_read8_slow(jlos_io8_slow_t *self)
{
    (void)self;
    return 0;
}

static void jlos_arm64_port_write16(jlos_io16_t *self, uint16_t val)
{
    (void)self; (void)val;
}

static uint16_t jlos_arm64_port_read16(jlos_io16_t *self)
{
    (void)self;
    return 0;
}

static void jlos_arm64_port_write32(jlos_io32_t *self, uint32_t val)
{
    (void)self; (void)val;
}

static uint32_t jlos_arm64_port_read32(jlos_io32_t *self)
{
    (void)self;
    return 0;
}

static const jlos_hal_io_ops_t jlos_hal_arm64_io_ops = {
    .init_io8         = jlos_arm64_port_init,
    .write_io8        = jlos_arm64_port_write8,
    .read_io8         = jlos_arm64_port_read8,
    .init_io8_slow    = jlos_arm64_port_init_slow,
    .write_io8_slow   = jlos_arm64_port_write8_slow,
    .read_io8_slow    = jlos_arm64_port_read8_slow,
    .init_io16        = jlos_arm64_port_init16,
    .write_io16       = jlos_arm64_port_write16,
    .read_io16        = jlos_arm64_port_read16,
    .init_io32        = jlos_arm64_port_init32,
    .write_io32       = jlos_arm64_port_write32,
    .read_io32        = jlos_arm64_port_read32,
    .read_mmio8       = jlos_arm64_mmio_read8,
    .read_mmio16      = jlos_arm64_mmio_read16,
    .read_mmio32      = jlos_arm64_mmio_read32,
    .write_mmio8      = jlos_arm64_mmio_write8,
    .write_mmio16     = jlos_arm64_mmio_write16,
    .write_mmio32     = jlos_arm64_mmio_write32,
};

const jlos_hal_io_ops_t *jlos_hal_io_ops = &jlos_hal_arm64_io_ops;
