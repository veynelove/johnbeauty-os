#ifndef __HDC_PCI_H
#define __HDC_PCI_H

#include <hdc/port.h>
#include <common/types.h>
#include <hdc/interrupts.h>
#include <drivers/driver.h>

namespace JLOS {
namespace Hdc {

enum BaseAddressRegisterType {
    MemoryMapping = 0,
    InputOutput = 1
};

class BaseAddressRegister {
public:
    bool prefetchable;
    uint8_t *address;
    uint32_t size;
    BaseAddressRegisterType type;
};

class PeripheralComponentInterconnectDeviceDesriptor {
public:
    uint32_t portBase;
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

    PeripheralComponentInterconnectDeviceDesriptor();
    ~PeripheralComponentInterconnectDeviceDesriptor();
};

class PeripheralComponentInterconnectController {
private:
    Port32Bit dataPort;
    Port32Bit commandPort;

public:
    PeripheralComponentInterconnectController();
    ~PeripheralComponentInterconnectController();

    uint32_t Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset);
    void Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset,
        uint32_t value);
    bool DeviceHasFunctions(uint16_t bus, uint16_t device);

    void SelectDrivers(Drivers::DriverManager *driverManager, InterruptManager *interrupts);
    Drivers::Driver *GetDriver(PeripheralComponentInterconnectDeviceDesriptor dev,
        InterruptManager *interrupts);
    PeripheralComponentInterconnectDeviceDesriptor GetDeviceDescriptor(uint16_t bus,
        uint16_t device, uint16_t function);
    BaseAddressRegister GetBaseAddressRegister(uint16_t bus, uint16_t device,
        uint16_t function, uint16_t bar);
};
}
}
#endif
