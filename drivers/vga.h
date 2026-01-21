#ifndef __DRIVERS_VGA_H
#define __DRIVERS_VGA_H

#include <common/types.h>
#include <hdc/interrupts.h>
#include <hdc/port.h>
#include <drivers/driver.h>

namespace JLOS {
namespace Drivers {
class video_graphics_array {
private:
     Hdc::port8_bit m_misc_port;
     Hdc::port8_bit m_crtc_index_port;
     Hdc::port8_bit m_crtc_data_port;
     Hdc::port8_bit m_sequencer_index_port;
     Hdc::port8_bit m_sequencer_data_port;
     Hdc::port8_bit m_graphics_controller_index_port;
     Hdc::port8_bit m_graphics_controller_data_port;
     Hdc::port8_bit m_attribute_controller_index_port;
     Hdc::port8_bit m_attribute_controller_read_port;
     Hdc::port8_bit m_attribite_controller_write_port;
     Hdc::port8_bit m_attribite_controller_reset_port;

     void write_registers(uint8_t *registers);
     uint8_t *get_frame_buffer_segment(uint8_t m_r, uint8_t m_g, uint8_t m_b);
     uint8_t *get_frame_buffer_segment();

     virtual uint8_t get_color_index(uint32_t m_r, uint32_t m_g, uint8_t m_b);

public:
     video_graphics_array();
     ~video_graphics_array();

     virtual bool support_mode(uint32_t width, uint32_t height, uint32_t colordepth);
     virtual bool set_mode(uint32_t width, uint32_t height, uint32_t colordepth);
     virtual void put_pixel(int32_t m_x, int32_t m_y, uint8_t m_r, uint8_t m_g, uint8_t m_b);
     virtual void put_pixel(int32_t m_x, int32_t m_y, uint8_t color_index);

     virtual void fill_rectangle(uint32_t m_x, uint32_t m_y, uint32_t m_w, uint32_t m_h, uint8_t m_r,
          uint8_t m_g, uint8_t m_b);
};
}
}
#endif
