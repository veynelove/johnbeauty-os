#ifndef __JLOS_HAL_IO_H
#define __JLOS_HAL_IO_H

#include <tools/config.h>
#include <common/types.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/port.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture I/O support not implemented yet"
#endif

/* I/O 统一命名：x86 port I/O / ARM MMIO load/store 上层透明 */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_port8_bit_t       jlos_io8_t;
  typedef jlos_port8_bit_slow_t  jlos_io8_slow_t;
  typedef jlos_port16_bit_t      jlos_io16_t;
  typedef jlos_port32_bit_t      jlos_io32_t;

  extern void jlos_io8_init(jlos_io8_t *self, uint16_t port);
  extern void jlos_io8_write(jlos_io8_t *self, uint8_t val);
  extern uint8_t jlos_io8_read(jlos_io8_t *self);

  extern void jlos_io8_slow_init(jlos_io8_slow_t *self, uint16_t port);
  extern void jlos_io8_slow_write(jlos_io8_slow_t *self, uint8_t val);
  extern uint8_t jlos_io8_slow_read(jlos_io8_slow_t *self);

  extern void jlos_io16_init(jlos_io16_t *self, uint16_t port);
  extern void jlos_io16_write(jlos_io16_t *self, uint16_t val);
  extern uint16_t jlos_io16_read(jlos_io16_t *self);

  extern void jlos_io32_init(jlos_io32_t *self, uint16_t port);
  extern void jlos_io32_write(jlos_io32_t *self, uint32_t val);
  extern uint32_t jlos_io32_read(jlos_io32_t *self);
#endif

#endif
