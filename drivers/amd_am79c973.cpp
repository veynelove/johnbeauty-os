#include <drivers/amd_am79c973.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printf_hex(uint8_t);
}

namespace JLOS {
namespace Drivers {
rawdata_handler::rawdata_handler(amd_am79c973 *backend)
{
     this->backend = backend;
     backend->set_handler(this);
}

rawdata_handler::~rawdata_handler()
{
     backend->set_handler(0);
}

bool rawdata_handler::on_raw_data_received(uint8_t *buffer, uint32_t m_size)
{
     return false;
}

void rawdata_handler::send(uint8_t *buffer, uint32_t m_size)
{

}

amd_am79c973::amd_am79c973(Hdc::peripheral_component_interconnect_device_desriptor *dev,
     Hdc::interrupt_manager *interrupts) : driver(),
     interrupt_handler(interrupts, dev->m_interrupt + interrupts->hardware_interrupt_offset()),
     m_mac_address0_port(dev->m_port_base),
     m_mac_address2_port(dev->m_port_base + 0x02),
     m_mac_address4_port(dev->m_port_base + 0x04),
     m_register_data_port(dev->m_port_base + 0x10),
     m_register_address_port(dev->m_port_base + 0x12),
     m_reset_port(dev->m_port_base + 0x14),
     m_bus_control_register_data_port(dev->m_port_base + 0x16)
{
     this->handler = 0;
     m_current_send_buffer = 0;
     m_current_recv_buffer = 0;

     uint64_t MAC0 = m_mac_address0_port.read() % 256;
     uint64_t MAC1 = m_mac_address0_port.read() / 256;
     uint64_t MAC2 = m_mac_address2_port.read() % 256;
     uint64_t MAC3 = m_mac_address2_port.read() / 256;
     uint64_t MAC4 = m_mac_address4_port.read() % 256;
     uint64_t MAC5 = m_mac_address4_port.read() / 256;
     uint64_t MAC = MAC5 << 40
                  | MAC4 << 32
                  | MAC3 << 24
                  | MAC2 << 16
                  | MAC1 << 8
                  | MAC0;
     //32 bit m_mode
     m_register_address_port.write(20);
     m_bus_control_register_data_port.write(0x102);

     //STOP reset
     m_register_address_port.write(0);
     m_register_data_port.write(0x04);

     //m_init_block
     m_init_block.m_mode = 0x0000; // promiscuous m_mode = false
     m_init_block.reserved1 = 0;
     m_init_block.num_send_buffers = 3;
     m_init_block.reserved2 = 0;
     m_init_block.num_recv_buffers = 3;
     m_init_block.physical_address = MAC;
     m_init_block.m_reserved3 = 0;
     m_init_block.m_logical_address = 0;

     send_buffer_descr = 
          (buffer_descriptor *)(((uint32_t)(&send_buffer_desc_memory[0]) + 15) & ~((uint32_t)0xF));
     m_init_block.m_send_buffer_descr_address = (uint32_t)send_buffer_descr;
     recv_buffer_descr = 
          (buffer_descriptor *)(((uint32_t)(&recv_buffer_desc_memory[0]) + 15) & ~((uint32_t)0xF));
     m_init_block.m_recv_buffer_descr_address = (uint32_t)recv_buffer_descr;

     for (uint8_t i = 0; i < 8; i++) {
          send_buffer_descr[i].m_address = (((uint32_t)&send_buffers[i]) + 15) & ~(uint32_t)0xF;
          send_buffer_descr[i].m_flags = (0x7FF | 0xF000);
          send_buffer_descr[i].m_flags2 = 0;
          send_buffer_descr[i].m_avail = 0;

          recv_buffer_descr[i].m_address = (((uint32_t)&recv_buffers[i]) + 15) & ~(uint32_t)0xF;
          recv_buffer_descr[i].m_flags = (0xF7FF | 0x80000000);
          recv_buffer_descr[i].m_flags2 = 0;
          recv_buffer_descr[i].m_avail = 0;
     }

     m_register_address_port.write(1);
     m_register_data_port.write((uint32_t)(&m_init_block) & 0xFFFF);
     m_register_address_port.write(2);
     m_register_data_port.write(((uint32_t)(&m_init_block) >> 16) & 0xFFFF);
}

amd_am79c973::~amd_am79c973(){}

void amd_am79c973::activate()
{
     m_register_address_port.write(0);
     m_register_data_port.write(0x41);

     m_register_address_port.write(4);
     uint32_t temp = m_register_data_port.read();
     m_register_data_port.write(4);
     m_register_data_port.write(temp | 0xC00);

     m_register_address_port.write(0);
     m_register_data_port.write(0x42);
}

int amd_am79c973::reset()
{
     m_reset_port.read();
     m_reset_port.write(0);
     return 10;
}

uint32_t amd_am79c973::handle_interrupt(uint32_t m_esp)
{
     Kernel::printf("INTERRUPT FROM AMD am79c973\n");
     m_register_address_port.write(0);
     uint32_t temp = m_register_data_port.read();
     if ((temp & 0x8000) == 0x8000) Kernel::printf("AMD am79c973 ERROR\n");
     if ((temp & 0x2000) == 0x2000) Kernel::printf("AMD am79c973 COLLISION ERROR\n");
     if ((temp & 0x1000) == 0x1000) Kernel::printf("AMD am79c973 MISSED ERROR\n");
     if ((temp & 0x0800) == 0x0800) Kernel::printf("AMD am79c973 MEMORY ERROR\n");
     if ((temp & 0x0400) == 0x0400) receive();
     if ((temp & 0x0200) == 0x0200) Kernel::printf("AMD am79c973 DATA SENT\n");

     // acknoledge
     m_register_address_port.write(0);
     m_register_data_port.write(temp);

     if ((temp & 0x0100) == 0x0100) Kernel::printf("AMD am79c973 INIT DONE\n");

     return m_esp;
}

void amd_am79c973::send(uint8_t *buffer, int m_size)
{
     int send_descriptor = m_current_send_buffer;
     m_current_send_buffer = (m_current_send_buffer + 1) % 8;
     if (m_size > 1518) {
          m_size = 1518;
     }
     for (uint8_t *src = buffer + m_size - 1, *dst =
          (uint8_t *)(send_buffer_descr[send_descriptor].m_address + m_size -1);
          src >= buffer; src--, dst--) 
     {
          *dst = *src;     
     }
     Kernel::printf("SEND: ");
     for (int i = (14 + 20); i < m_size; i++) {
          Kernel::printf_hex(buffer[i]);
          Kernel::printf(" ");
     }
     send_buffer_descr[send_descriptor].m_avail = 0;
     send_buffer_descr[send_descriptor].m_flags2 = 0;
     send_buffer_descr[send_descriptor].m_flags = 0x8300F000 | ((uint16_t)((-m_size) & 0xFFF));
     m_register_address_port.write(0);
     m_register_data_port.write(0x48);
}

void amd_am79c973::receive()
{
     Kernel::printf("RECV\n");
     for (; (recv_buffer_descr[m_current_recv_buffer].m_flags & 0x80000000) == 0;
          m_current_recv_buffer = (m_current_recv_buffer + 1) % 8) {
          if (!(recv_buffer_descr[m_current_recv_buffer].m_flags & 0x40000000)
             && (recv_buffer_descr[m_current_recv_buffer].m_flags & 0x03000000) == 0x03000000) {
               uint32_t m_size = recv_buffer_descr[m_current_recv_buffer].m_flags & 0xFFF;
               if (m_size > 64) { //remove m_checksum
                    m_size -=4;
               }
               uint8_t *buffer = (uint8_t *)(recv_buffer_descr[m_current_recv_buffer].m_address);
               for (int i = (14 + 20); i < m_size; i++) {
                    Kernel::printf_hex(buffer[i]);
                    Kernel::printf(" ");
               }
               if (handler) {
                    if (handler->on_raw_data_received(buffer, m_size)) {
                         send(buffer, m_size);
                    }
               }
               m_size = 64;
          }
          recv_buffer_descr[m_current_recv_buffer].m_flags2 = 0;
          recv_buffer_descr[m_current_recv_buffer].m_flags = 0x8000F7FF;
     }
}

void amd_am79c973::set_handler(rawdata_handler *handler)
{
     this->handler = handler;
}

uint64_t amd_am79c973::get_mac_address()
{
     return m_init_block.physical_address;
}

void amd_am79c973::set_ip_address(uint32_t ip)
{
     m_init_block.m_logical_address = ip;
}

uint32_t amd_am79c973::get_ip_address()
{
     return m_init_block.m_logical_address;
}
}
}