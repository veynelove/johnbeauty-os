#ifndef __JLOS_HAL_IO_H
#define __JLOS_HAL_IO_H

#include <tools/config.h>
#include <common/types.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/port.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture I/O support not implemented yet"
#endif

/* ============================================================
 *  Unified Type Naming (io instead of x86-specific "port")
 *  x86 uses port I/O (in/out instructions)
 *  ARM uses memory-mapped I/O (MMIO: load/store to addresses)
 * ============================================================ */
#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
  typedef jlos_port8_bit_t       jlos_io8_t;
  typedef jlos_port8_bit_slow_t  jlos_io8_slow_t;
  typedef jlos_port16_bit_t      jlos_io16_t;
  typedef jlos_port32_bit_t      jlos_io32_t;

  /* --- 8-bit I/O --- */
  #define jlos_io8_init(port, addr)       jlos_port8_bit_init(port, addr)
  #define jlos_io8_write(port, val)       jlos_port8_bit_write(port, val)
  #define jlos_io8_read(port)             jlos_port8_bit_read(port)

  /* --- 8-bit slow I/O (with tiny delay for old hardware) --- */
  #define jlos_io8_slow_init(port, addr)  jlos_port8_bit_slow_init(port, addr)
  #define jlos_io8_slow_write(port, val)  jlos_port8_bit_slow_write(port, val)
  #define jlos_io8_slow_read(port)        jlos_port8_bit_slow_read(port)

  /* --- 16-bit I/O --- */
  #define jlos_io16_init(port, addr)      jlos_port16_bit_init(port, addr)
  #define jlos_io16_write(port, val)      jlos_port16_bit_write(port, val)
  #define jlos_io16_read(port)            jlos_port16_bit_read(port)

  /* --- 32-bit I/O --- */
  #define jlos_io32_init(port, addr)      jlos_port32_bit_init(port, addr)
  #define jlos_io32_write(port, val)      jlos_port32_bit_write(port, val)
  #define jlos_io32_read(port)            jlos_port32_bit_read(port)
#endif

/* ============================================================
 *  Unified API Contract
 *
 *  Any architecture port MUST provide:
 *    TYPES:
 *      - jlos_io8_t,  jlos_io8_slow_t
 *      - jlos_io16_t, jlos_io32_t
 *
 *    FUNCTIONS (pattern: jlos_io<width>[_slow]_<op>):
 *      - void   jlos_ioN_init  (jlos_ioN_t*, uint32_t address)
 *      - void   jlos_ioN_write (jlos_ioN_t*, uintN_t value)
 *      - uintN_t jlos_ioN_read (jlos_ioN_t*)
 *    Where N ∈ {8, 16, 32} and optional _slow suffix applies.
 * ============================================================ */

#endif
