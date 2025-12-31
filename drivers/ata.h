#ifndef __JLOS__DRIVERS_ATA_H
#define __JLOS__DRIVERS_ATA_H

#include <common/types.h>
#include <hdc/port.h>

namespace JLOS {
namespace Drivers {
class AdvancedTechnologAttachment {
private:
     Hdc::Port16Bit dataPort;
     Hdc::Port8Bit errorPort;
     Hdc::Port8Bit sectorCountPort;
     Hdc::Port8Bit lbaLowPort;
     Hdc::Port8Bit lbaMidPort;
     Hdc::Port8Bit lbaHiPort;
     Hdc::Port8Bit devicePort;
     Hdc::Port8Bit commandPort;
     Hdc::Port8Bit controlPort;

     bool master;
     uint16_t bytesPerSector;

public:
     AdvancedTechnologAttachment(uint16_t portBase, bool master);
     ~AdvancedTechnologAttachment();

     void Identify();
     void Read28(uint32_t sector, uint8_t *data, int size);
     void Write28(uint32_t sector, uint8_t *data, int size);
     void Flush();
};
}
}
#endif
