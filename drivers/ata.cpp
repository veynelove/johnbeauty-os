#include <drivers/ata.h>

namespace JLOS::Kernel {
void printf(const char *str);
}

namespace JLOS {
namespace Drivers {

advanced_technolog_attachment::advanced_technolog_attachment(uint16_t m_port_base, bool m_master)
: m_data_port(m_port_base), m_error_port(m_port_base + 1), m_sector_count_port(m_port_base + 2),
m_lba_low_port(m_port_base + 3), m_lba_mid_port(m_port_base + 4), m_lba_hi_port(m_port_base + 5),
m_device_port(m_port_base + 6), m_command_port(m_port_base + 7), m_control_port(m_port_base + 0x206)
{
     m_bytes_per_sector = 512;
     this->m_master = m_master;
}

advanced_technolog_attachment::~advanced_technolog_attachment(){}

void advanced_technolog_attachment::identify()
{
     m_device_port.write(m_master ? 0xA0 : 0xB0);
     m_control_port.write(0);

     m_device_port.write(0xA0);
     uint8_t status = m_command_port.read();
     if (status == 0xFF) {
          return;
     }
     m_device_port.write(m_master ? 0xA0 : 0xB0);
     m_sector_count_port.write(0);
     m_lba_low_port.write(0);
     m_lba_mid_port.write(0);
     m_lba_hi_port.write(0);
     m_command_port.write(0xEC);

     status = m_command_port.read();
     if (status == 0x00) {
          return; // no m_device
     }
     while (((status & 0x80) == 0x80)
          && ((status & 0x01) != 0x01)) {
          status = m_command_port.read();
     }
     if (status & 0x01) {
          Kernel::printf("ERROR");
          return;
     }
     for (uint16_t i = 0; i < 256; i++) {
          uint16_t m_data = m_data_port.read();
          char *foo = " \0";
          foo[1] = (m_data >> 8) & 0x00FF;
          foo[0] = m_data & 0x00FF;
          Kernel::printf(foo);
     }
}

void advanced_technolog_attachment::read28(uint32_t sector, uint8_t *m_data, int m_size)
{
     if (sector & 0xF0000000) {
          return;
     }
     if (m_size > m_bytes_per_sector) {
          return;
     }
     m_device_port.write((m_master ? 0xE0 : 0xF0) | (sector & 0xF0000000) >> 24);
     m_error_port.write(0);
     m_sector_count_port.write(1);

     m_lba_low_port.write(sector & 0x000000FF);
     m_lba_mid_port.write((sector & 0x000000FF) >> 8);
     m_lba_hi_port.write((sector & 0x000000FF) >> 16);
     m_command_port.write(0x20);

     uint8_t status = m_command_port.read();
     while (((status & 0x80) == 0x80)
          && ((status & 0x01) != 0x01)) {
          status = m_command_port.read();
     }
     if (status & 0x01) {
          Kernel::printf("ERROR");
          return;
     }
     Kernel::printf("reading from ATA.");
     for (uint16_t i = 0; i < m_size; i += 2) {
          uint16_t wdata = m_data_port.read();
          /**
          char *foo = " \0";
          foo[1] = (wdata >> 8) & 0x00FF;
          foo[0] = wdata & 0x00FF;
          Kernel::printf(foo);
          */
          m_data[i] = wdata & 0x00FF;
          if (i + 1 < m_size) {
               m_data[i + 1] = (wdata >> 8) & 0x00FF;
          }
     }
     for (uint16_t i = m_size + (m_size % 2); i < m_bytes_per_sector; i += 2) {
          m_data_port.read();
     }
}

void advanced_technolog_attachment::write28(uint32_t sector, uint8_t *m_data, int m_size)
{
     if (sector & 0xF0000000) {
          return;
     }
     if (m_size > m_bytes_per_sector) {
          return;
     }
     m_device_port.write((m_master ? 0xE0 : 0xF0) | (sector & 0xF0000000) >> 24);
     m_error_port.write(0);
     m_sector_count_port.write(1);

     m_lba_low_port.write(sector & 0x000000FF);
     m_lba_mid_port.write((sector & 0x000000FF) >> 8);
     m_lba_hi_port.write((sector & 0x000000FF) >> 16);
     m_command_port.write(0x30);

     Kernel::printf("writing to ATA.");
     for (uint16_t i = 0; i < m_size; i += 2) {
          uint16_t wdata = m_data[i];
          if (i + 1 < m_size) {
               wdata |= ((uint16_t)m_data[i + 1]) << 8;
          }
          char *foo = " \0";
          foo[1] = (wdata >> 8) & 0x00FF;
          foo[0] = wdata & 0x00FF;
          Kernel::printf(foo);
          m_data_port.write(wdata);
     }
     for (uint16_t i = m_size + (m_size % 2); i < m_bytes_per_sector; i += 2) {
          m_data_port.write(0x0000);
     }
}

void advanced_technolog_attachment::flush()
{
     m_device_port.write(m_master ? 0xE0 : 0xF0);
     m_command_port.write(0xE7);

     uint8_t status = m_command_port.read();
     while (((status & 0x80) == 0x80)
          && ((status & 0x01) != 0x01)) {
          status = m_command_port.read();
     }
     if (status & 0x01) {
          Kernel::printf("ERROR");
     }
}
}
}
