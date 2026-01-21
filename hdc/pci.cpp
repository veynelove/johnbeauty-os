#include <hdc/pci.h>
#include <drivers/amd_am79c973.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printf_hex(uint8_t);
}

namespace JLOS {
namespace Hdc {
peripheral_component_interconnect_device_desriptor
::peripheral_component_interconnect_device_desriptor(){}

peripheral_component_interconnect_device_desriptor
::~peripheral_component_interconnect_device_desriptor(){}

peripheral_component_interconnect_controller::peripheral_component_interconnect_controller()
:m_data_port(0xCFC), m_command_port(0xCF8){}

peripheral_component_interconnect_controller::~peripheral_component_interconnect_controller(){}

uint32_t peripheral_component_interconnect_controller::read(uint16_t m_bus, uint16_t m_device,
    uint16_t m_function, uint32_t registeroffset)
{
    uint32_t id = 0x1 <<31
        | ((m_bus & 0xFF) << 16)
        | ((m_device & 0x1F) << 11)
        | ((m_function & 0x07) << 8)
        | (registeroffset & 0xFC);
    
    m_command_port.write(id);
    uint32_t result = m_data_port.read();
    return result >> (8 * (registeroffset % 4));
}

void peripheral_component_interconnect_controller::write(uint16_t m_bus, uint16_t m_device,
    uint16_t m_function, uint32_t registeroffset, uint32_t value)
{
    uint32_t id = 0x1 <<31
        | ((m_bus & 0xFF) << 16)
        | ((m_device & 0x1F) << 11)
        | ((m_function & 0x07) << 8)
        | (registeroffset & 0xFC);
    
    m_command_port.write(id);
    m_data_port.write(value);
}

bool peripheral_component_interconnect_controller::device_has_functions(uint16_t m_bus, uint16_t m_device)
{
    return read(m_bus, m_device, 0, 0x0E) & (1<<7);
}

void peripheral_component_interconnect_controller::select_drivers(Drivers::driver_manager *driver_manager,
    interrupt_manager *interrupts)
{
    for (int m_bus = 0; m_bus < 8; m_bus++) {
        for (int m_device = 0; m_device < 32; m_device++) {
            int num_functions = device_has_functions(m_bus, m_device) ? 8 : 1;
            for (int m_function = 0; m_function < num_functions; m_function++) {
                peripheral_component_interconnect_device_desriptor dev =
                    get_device_descriptor(m_bus, m_device, m_function);

                if (dev.m_vendor_id == 0x0000 || dev.m_vendor_id == 0xFFFF) {
                    continue;
                }
                for (int bar_num = 0; bar_num < 6; bar_num++) {
                    base_address_register bar =
                        get_base_address_register(m_bus, m_device, m_function, bar_num);
                    if (bar.m_address && (bar.m_type == input_output)) {
                        dev.m_port_base = (uint32_t)bar.m_address;
                    }
                }
                Drivers::driver *driver = get_driver(dev, interrupts);
                if (driver != 0) {
                    driver_manager->add_driver(driver);
                }
                /**
                Kernel::printf("PCI BUS ");
                Kernel::printf_hex(m_bus & 0xFF);
                Kernel::printf(", DEVICE ");
                Kernel::printf_hex(m_device & 0xFF);
                Kernel::printf(", FUNCTION ");
                Kernel::printf_hex(m_function & 0xFF);
                Kernel::printf(" = VENDOR ");
                Kernel::printf_hex((dev.m_vendor_id & 0xFF00) >> 8);
                Kernel::printf_hex(dev.m_vendor_id & 0xFF);
                Kernel::printf(", DEVICE ");
                Kernel::printf_hex((dev.m_device_id & 0xFF00) >> 8);
                Kernel::printf_hex(dev.m_device_id & 0xFF);
                Kernel::printf("\n");
                */
            }
        }
    }
}

base_address_register peripheral_component_interconnect_controller
::get_base_address_register(uint16_t m_bus, uint16_t m_device, uint16_t m_function, uint16_t bar)
{
    base_address_register result;
    uint32_t headertype = read(m_bus, m_device, m_function, 0x0E) & 0x7F;
    int maxbars = 6 - (4*headertype);
    if (bar >= maxbars) {
        return result;
    }
    uint32_t bar_value = read(m_bus, m_device, m_function, 0x10 + 4*bar);
    result.m_type = (bar_value & 0x1) ? input_output : memory_mapping;
    uint32_t temp;
    if (result.m_type == memory_mapping) {
        switch (((bar_value >> 1) && 0x3)) {
            case 0: //32 bit_mode;
            case 1: //32 bit_mode;
            case 2: //32 bit_mode;
            break;
        }
    } else { //input_output
        result.m_address = (uint8_t *)(bar_value & ~0x3);
        result.m_prefetchable = false;
    }
    return result;
}

Drivers::driver *peripheral_component_interconnect_controller
::get_driver(peripheral_component_interconnect_device_desriptor dev, interrupt_manager *interrupts)
{
    Drivers::driver *driver = nullptr;
    switch ((dev.m_vendor_id)) {
        case 0x1022: //AMD
            switch (dev.m_device_id) {
                case 0x2000: //am79c973
                    driver = (Drivers::amd_am79c973 *)Kernel::memory_manager::
                        active_memory_manager->malloc(sizeof(Drivers::amd_am79c973));
                    if (driver) {
                        new (driver) Drivers::amd_am79c973(&dev, interrupts);
                        return driver;
                    }
                    Kernel::printf("AMD am79c973 ");
                    break;
            }
            break;
        case 0x8086: //intel
            break;
    }
    switch (dev.m_class_id) {
        case 0x03: //graphics
            switch (dev.m_subclass_id) {
                case 0x00: //VGA
                    Kernel::printf("VGA\n");
                    break;
            }
            break;
    }
    return 0;
}

peripheral_component_interconnect_device_desriptor peripheral_component_interconnect_controller
::get_device_descriptor(uint16_t m_bus, uint16_t m_device, uint16_t m_function)
{
    peripheral_component_interconnect_device_desriptor result;
    result.m_bus = m_bus;
    result.m_device = m_device;
    result.m_function = m_function;

    result.m_vendor_id = read(m_bus, m_device, m_function, 0x00);
    result.m_device_id = read(m_bus, m_device, m_function, 0x02);

    result.m_class_id = read(m_bus, m_device, m_function, 0x0b);
    result.m_subclass_id = read(m_bus, m_device, m_function, 0x0a);
    result.m_interface_id = read(m_bus, m_device, m_function, 0x09);

    result.m_revision = read(m_bus, m_device, m_function, 0x08);
    result.m_interrupt = read(m_bus, m_device, m_function, 0x3c);

    return result; 
}
}
}
