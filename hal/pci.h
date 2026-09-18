#ifndef _JLOS_HAL_PCI_H
#define _JLOS_HAL_PCI_H

#include <common/types.h>

#define JLOS_PCI_CFG_ADDR_ENABLE 0x80000000u

typedef enum {
    JLOS_HAL_PCI_MEMORY_MAPPING = 0,
    JLOS_HAL_PCI_INPUT_OUTPUT   = 1
} jlos_hal_pci_bar_type_t;

typedef struct {
    bool                    prefetchable;
    uint8_t                 *address;
    uint32_t                size;
    jlos_hal_pci_bar_type_t type;
} jlos_hal_pci_bar_t;

typedef struct {
    uint32_t    port_base;
    uint32_t    interrupt;
    uint16_t    bus;
    uint16_t    device;
    uint16_t    function;
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint8_t     class_id;
    uint8_t     subclass_id;
    uint8_t     interface_id;
    uint8_t     revision;
} jlos_hal_pci_device_t;

typedef struct jlos_hal_pci_controller jlos_hal_pci_controller_t;

void jlos_hal_pci_init(void);

uint32_t jlos_hal_pci_config_read32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg);
uint16_t jlos_hal_pci_config_read16(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg);
uint8_t  jlos_hal_pci_config_read8 (jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg);
void jlos_hal_pci_config_write32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint32_t val);
void jlos_hal_pci_config_write16(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint16_t val);
void jlos_hal_pci_config_write8 (jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint8_t val);

void jlos_hal_pci_enumerate_and_bind_drivers(void);

jlos_hal_pci_device_t jlos_hal_pci_get_device_descriptor(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func);
jlos_hal_pci_bar_t jlos_hal_pci_get_base_address_register(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t bar_index);

#endif
