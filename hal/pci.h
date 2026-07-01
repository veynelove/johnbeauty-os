#ifndef __JLOS_HAL_PCI_H
#define __JLOS_HAL_PCI_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/io.h>
#include <hal/irq.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/pci.h>
#elif KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_ARM
#error "ARM architecture PCI support not implemented yet"
#endif

/* ============================================================
 *  PCI is a cross-vendor standard, so names are kept as-is.
 *  We only make the jlos_irq_manager_t visible to hal-level
 *  consumers so they don't have to include arch/x86/interrupts.h.
 *  (jlos_pci_controller_t internally uses jlos_io32_t via hal/io.h)
 * ============================================================ */

#endif
