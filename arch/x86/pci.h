#ifndef __HDC_PCI_H
#define __HDC_PCI_H

#include <arch/x86/port.h>
#include <common/types.h>

typedef struct jlos_interrupt_manager jlos_interrupt_manager_t;
typedef struct jlos_driver_manager jlos_driver_manager_t;
typedef struct jlos_driver jlos_driver_t;
typedef struct jlos_amd_am79c973 jlos_amd_am79c973_t;

typedef enum {
    JLOS_PCI_MEMORY_MAPPING = 0,
    JLOS_PCI_INPUT_OUTPUT = 1
} jlos_pci_bar_type_t;

typedef struct {
    bool prefetchable;
    uint8_t *address;
    uint32_t size;
    jlos_pci_bar_type_t type;
} jlos_pci_bar_t;

typedef struct {
    uint32_t port_base;
    uint32_t interrupt;
    
    uint16_t bus;
    uint16_t device;
    uint16_t function;

    uint16_t vendor_id;
    uint16_t device_id;
    
    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t interface_id;

    uint8_t revision;
} jlos_pci_device_descriptor_t;

typedef struct {
    jlos_port32_bit_t data_port;
    jlos_port32_bit_t command_port;
} jlos_pci_controller_t;

void jlos_pci_controller_init(jlos_pci_controller_t* self);

uint32_t jlos_pci_controller_read(jlos_pci_controller_t* self, uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset);
void jlos_pci_controller_write(jlos_pci_controller_t* self, uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value);
bool jlos_pci_controller_device_has_functions(jlos_pci_controller_t* self, uint16_t bus, uint16_t device);

void jlos_pci_controller_select_drivers(jlos_pci_controller_t* self, jlos_driver_manager_t *driver_manager, jlos_interrupt_manager_t *interrupts);
jlos_driver_t *jlos_pci_controller_get_driver(jlos_pci_controller_t* self, jlos_pci_device_descriptor_t dev, jlos_interrupt_manager_t *interrupts);
jlos_driver_t *jlos_pci_network_controller_handle(jlos_pci_controller_t* self, jlos_pci_device_descriptor_t dev, jlos_interrupt_manager_t *interrupts);

jlos_pci_device_descriptor_t jlos_pci_controller_get_device_descriptor(jlos_pci_controller_t* self, uint16_t bus, uint16_t device, uint16_t function);
jlos_pci_bar_t jlos_pci_controller_get_base_address_register(jlos_pci_controller_t* self, uint16_t bus, uint16_t device, uint16_t function, uint16_t bar);
#endif