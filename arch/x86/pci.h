#ifndef _JLOS_ARCH_X86_PCI_H
#define _JLOS_ARCH_X86_PCI_H

#include <hal/pci.h>
#include <arch/x86/io.h>

typedef struct jlos_hal_pci_controller {
    jlos_io32_t data_port;
    jlos_io32_t command_port;
} jlos_hal_pci_controller_t;

#endif