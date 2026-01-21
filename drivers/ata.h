#ifndef __JLOS__DRIVERS_ATA_H
#define __JLOS__DRIVERS_ATA_H

#include <common/types.h>
#include <hdc/port.h>

namespace JLOS {
namespace Drivers {
class advanced_technolog_attachment {
private:
     Hdc::port16_bit m_data_port;
     Hdc::port8_bit m_error_port;
     Hdc::port8_bit m_sector_count_port;
     Hdc::port8_bit m_lba_low_port;
     Hdc::port8_bit m_lba_mid_port;
     Hdc::port8_bit m_lba_hi_port;
     Hdc::port8_bit m_device_port;
     Hdc::port8_bit m_command_port;
     Hdc::port8_bit m_control_port;

     bool m_master;
     uint16_t m_bytes_per_sector;

public:
     advanced_technolog_attachment(uint16_t m_port_base, bool m_master);
     ~advanced_technolog_attachment();

     void identify();
     void read28(uint32_t sector, uint8_t *m_data, int m_size);
     void write28(uint32_t sector, uint8_t *m_data, int m_size);
     void flush();
};
}
}
#endif
