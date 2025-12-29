#ifndef __JLOS_DRIVERS_AMD_AM79C973_H
#define __JLOS_DRIVERS_AMD_AM79C973_H

#include <drivers/driver.h>
#include <hdc/interrupts.h>
#include <hdc/pci.h>
#include <hdc/port.h>

namespace JLOS {
namespace Drivers {
class amd_am79c973 : public Driver, public Hdc::InterruptHandler {
private:
     struct InitializationBlock {
          uint16_t mode;
          unsigned reserved1 : 4;
          unsigned numSendBuffers : 4;
          unsigned reserved2 : 4;
          unsigned numRecvBuffers : 4;
          uint64_t physicalAddress : 48;
          uint16_t reserved3;
          uint64_t logicalAddress;
          uint32_t recvBufferDescrAddress;
          uint32_t sendBufferDescrAddress;
     } __attribute__((packed));

     struct BufferDescriptor {
          uint32_t address;
          uint32_t flags;
          uint32_t flags2;
          uint32_t avail;
     } __attribute__((packed));

private:
     Hdc::Port16Bit MACAddress0Port;
     Hdc::Port16Bit MACAddress2Port;
     Hdc::Port16Bit MACAddress4Port;
     Hdc::Port16Bit registerDataPort;
     Hdc::Port16Bit registerAddressPort;
     Hdc::Port16Bit resetPort;
     Hdc::Port16Bit busControlRegisterDataPort;

     InitializationBlock initBlock;

     BufferDescriptor *sendBufferDescr;
     uint8_t sendBufferDescMemory[2048 + 15];
     uint8_t sendBuffers[2 * 1024 + 15][8];
     uint8_t currentSendBuffer;

     BufferDescriptor *recvBufferDescr;
     uint8_t recvBufferDescMemory[2048 + 15];
     uint8_t recvBuffers[2 * 1024 + 15][8];
     uint8_t currentRecvBuffer;

public:
     amd_am79c973(Hdc::PeripheralComponentInterconnectDeviceDesriptor *dev,
          Hdc::InterruptManager *interrupts);
     ~amd_am79c973();

     void Activate() override;
     int Reset() override;
     uint32_t HandleInterrupt(uint32_t esp) override;
};
}
}

#endif
