#include <hal/io.h>
#include <hal/hal.h>
#include <hal/diag.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

#define HAL_IO_OWNER  "(HAL io layer)"

static inline const jlos_hal_io_ops_t *ops_safe(void)
{
    if (!jlos_hal_io_ops) return &jlos_hal_x86_fast_io_ops;
    return jlos_hal_io_ops;
}

void jlos_io8_init(jlos_io8_t *self, uint16_t port)
{ ops_safe()->init_io8(self, port); }
void jlos_io8_write(jlos_io8_t *self, uint8_t val)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 1, HAL_IO_OWNER);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR8, self->m_portnumber, val);
    ops_safe()->write_io8(self, val);
}
uint8_t jlos_io8_read(jlos_io8_t *self)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 0, HAL_IO_OWNER);
    uint8_t v = ops_safe()->read_io8(self);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_RD8, self->m_portnumber, v);
    return v;
}

void jlos_io8_slow_init(jlos_io8_slow_t *self, uint16_t port)
{ ops_safe()->init_io8_slow(self, port); }
void jlos_io8_slow_write(jlos_io8_slow_t *self, uint8_t val)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 1, HAL_IO_OWNER);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR8, self->m_portnumber, val);
    ops_safe()->write_io8_slow(self, val);
}
uint8_t jlos_io8_slow_read(jlos_io8_slow_t *self)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 0, HAL_IO_OWNER);
    uint8_t v = ops_safe()->read_io8_slow(self);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_RD8, self->m_portnumber, v);
    return v;
}

void jlos_io16_init(jlos_io16_t *self, uint16_t port)
{ ops_safe()->init_io16(self, port); }
void jlos_io16_write(jlos_io16_t *self, uint16_t val)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 1, HAL_IO_OWNER);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR16, self->m_portnumber, val);
    ops_safe()->write_io16(self, val);
}
uint16_t jlos_io16_read(jlos_io16_t *self)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 0, HAL_IO_OWNER);
    uint16_t v = ops_safe()->read_io16(self);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_RD16, self->m_portnumber, v);
    return v;
}

void jlos_io32_init(jlos_io32_t *self, uint16_t port)
{ ops_safe()->init_io32(self, port); }
void jlos_io32_write(jlos_io32_t *self, uint32_t val)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 1, HAL_IO_OWNER);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR32, self->m_portnumber, val);
    ops_safe()->write_io32(self, val);
}
uint32_t jlos_io32_read(jlos_io32_t *self)
{
    jlos_hal_io_sanity_check(self->m_portnumber, 0, HAL_IO_OWNER);
    uint32_t v = ops_safe()->read_io32(self);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_RD32, self->m_portnumber, v);
    return v;
}

#endif
