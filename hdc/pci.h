#ifndef __HDC_PCI_H
#define __HDC_PCI_H

#include <hdc/port.h>
#include <common/types.h>
#include <hdc/interrupts.h>
#include <drivers/driver.h>
#include <kernel/memory_manager.h>

namespace JLOS {
namespace Hdc {

enum base_address_register_type {
    memory_mapping = 0,
    input_output = 1
};

class base_address_register {
public:
    bool m_prefetchable;
    uint8_t *m_address;
    uint32_t m_size;
    base_address_register_type m_type;
};

class peripheral_component_interconnect_device_desriptor {
public:
    uint32_t m_port_base;
    uint32_t m_interrupt;
    
    uint16_t m_bus;
    uint16_t m_device;
    uint16_t m_function;

    uint16_t m_vendor_id;
    uint16_t m_device_id;
    
    uint8_t m_class_id;
    uint8_t m_subclass_id;
    uint8_t m_interface_id;

    uint8_t m_revision;

    peripheral_component_interconnect_device_desriptor();
    ~peripheral_component_interconnect_device_desriptor();
};

class peripheral_component_interconnect_controller {
private:
    port32_bit m_data_port;
    port32_bit m_command_port;

public:
    peripheral_component_interconnect_controller();
    ~peripheral_component_interconnect_controller();

    uint32_t read(uint16_t m_bus, uint16_t m_device, uint16_t m_function, uint32_t registeroffset);
    void write(uint16_t m_bus, uint16_t m_device, uint16_t m_function, uint32_t registeroffset,
        uint32_t value);
    bool device_has_functions(uint16_t m_bus, uint16_t m_device);

    void select_drivers(Drivers::driver_manager *driver_manager, interrupt_manager *interrupts);
    Drivers::driver *get_driver(peripheral_component_interconnect_device_desriptor dev,
        interrupt_manager *interrupts);
    peripheral_component_interconnect_device_desriptor get_device_descriptor(uint16_t m_bus,
        uint16_t m_device, uint16_t m_function);
    base_address_register get_base_address_register(uint16_t m_bus, uint16_t m_device,
        uint16_t m_function, uint16_t bar);
};
}
}
#endif
