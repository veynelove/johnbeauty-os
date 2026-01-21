#ifndef __JLOS_DRIVERS_AMD_AM79C973_H
#define __JLOS_DRIVERS_AMD_AM79C973_H

#include <drivers/driver.h>
#include <hdc/interrupts.h>
#include <hdc/pci.h>
#include <hdc/port.h>

namespace JLOS {
namespace Drivers {
class amd_am79c973;

class rawdata_handler {
protected:
     amd_am79c973 *backend;

public:
     rawdata_handler(amd_am79c973 *backend);
     ~rawdata_handler();

     virtual bool on_raw_data_received(uint8_t *buffer, uint32_t m_size);
     void send(uint8_t *buffer, uint32_t m_size);
};

class amd_am79c973 : public driver, public Hdc::interrupt_handler {
private:
     struct initialization_block {
          uint16_t m_mode;
          unsigned reserved1 : 4;
          unsigned num_send_buffers : 4;
          unsigned reserved2 : 4;
          unsigned num_recv_buffers : 4;
          uint64_t physical_address : 48;
          uint16_t m_reserved3;
          uint64_t m_logical_address;
          uint32_t m_recv_buffer_descr_address;
          uint32_t m_send_buffer_descr_address;
     } __attribute__((packed));

     struct buffer_descriptor {
          uint32_t m_address;
          uint32_t m_flags;
          uint32_t m_flags2;
          uint32_t m_avail;
     } __attribute__((packed));

private:
     Hdc::port16_bit m_mac_address0_port;
     Hdc::port16_bit m_mac_address2_port;
     Hdc::port16_bit m_mac_address4_port;
     Hdc::port16_bit m_register_data_port;
     Hdc::port16_bit m_register_address_port;
     Hdc::port16_bit m_reset_port;
     Hdc::port16_bit m_bus_control_register_data_port;

     initialization_block m_init_block;

     buffer_descriptor *send_buffer_descr;
     uint8_t send_buffer_desc_memory[2048 + 15];
     uint8_t send_buffers[2 * 1024 + 15][8];
     uint8_t m_current_send_buffer;

     buffer_descriptor *recv_buffer_descr;
     uint8_t recv_buffer_desc_memory[2048 + 15];
     uint8_t recv_buffers[2 * 1024 + 15][8];
     uint8_t m_current_recv_buffer;

     rawdata_handler *handler;

public:
     amd_am79c973(Hdc::peripheral_component_interconnect_device_desriptor *dev,
          Hdc::interrupt_manager *interrupts);
     ~amd_am79c973();

     void activate() override;
     int reset() override;
     uint32_t handle_interrupt(uint32_t m_esp) override;

     void send(uint8_t *buffer, int m_size);
     void receive();
     void set_handler(rawdata_handler *handler);

     uint64_t get_mac_address();
     void set_ip_address(uint32_t);
     uint32_t get_ip_address();
};
}
}

#endif
