#include <drivers/vga.h>

namespace JLOS {
namespace Drivers {
video_graphics_array::video_graphics_array():
m_misc_port(0x3c2),m_crtc_index_port(0x3d4),m_crtc_data_port(0x3d5), m_sequencer_index_port(0x3c4),
m_sequencer_data_port(0x3c5), m_graphics_controller_index_port(0x3ce), m_graphics_controller_data_port(0x3cf),
m_attribute_controller_index_port(0x3c0), m_attribute_controller_read_port(0x3c1),
m_attribite_controller_write_port(0x3c0), m_attribite_controller_reset_port(0x3da){}

video_graphics_array::~video_graphics_array(){}

void video_graphics_array::write_registers(uint8_t *registers)
{
     m_misc_port.write(*(registers++));
     for (uint8_t i = 0; i < 5; i++) {
          m_sequencer_index_port.write(i);
          m_sequencer_data_port.write(*(registers++));
     }
     //cathode ray tube controller
     m_crtc_index_port.write(0x03);
     m_crtc_data_port.write(m_crtc_data_port.read() | 0x80);
     m_crtc_index_port.write(0x11);
     m_crtc_data_port.write(m_crtc_data_port.read() & ~0x80);

     registers[0x03] = registers[0x03] | 0x80;
     registers[0x11] = registers[0x11] & ~0x80;

     for (uint8_t i = 0; i < 25; i++) {
          m_crtc_index_port.write(i);
          m_crtc_data_port.write(*(registers++));
     }
     //graphics controller
     for (uint8_t i = 0; i < 9; i++) {
          m_graphics_controller_index_port.write(i);
          m_graphics_controller_data_port.write(*(registers++));
     }
     //attribute controller
     for (uint8_t i = 0; i < 21; i++) {
          m_attribite_controller_reset_port.read();
          m_attribute_controller_index_port.write(i);
          m_attribite_controller_write_port.write(*(registers++));
     }
     m_attribite_controller_reset_port.read();
     m_attribute_controller_index_port.write(0x20);
}

bool video_graphics_array::support_mode(uint32_t width, uint32_t height, uint32_t colordepth)
{
     return width == 320 && height == 200 && colordepth == 8;
}

bool video_graphics_array::set_mode(uint32_t width, uint32_t height, uint32_t colordepth)
{
     if (!support_mode(width, height, colordepth)) {
          return false;
     }
     uint8_t g_320x200x256[] = {
          /* MISC */
               0x63,
          /* SEQ */
               0x03, 0x01, 0x0F, 0x00, 0x0E,
          /* CRTC */
               0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
               0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x9C, 0x0E, 0x8F, 0x28,	0x40, 0x96, 0xB9, 0xA3,
               0xFF,
          /* GC */
               0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
               0xFF,
          /* AC */
               0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
               0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
               0x41, 0x00, 0x0F, 0x00, 0x00
     };
     write_registers(g_320x200x256);
     return true;
}

uint8_t *video_graphics_array::get_frame_buffer_segment(uint8_t m_r, uint8_t m_g, uint8_t m_b)
{
     m_graphics_controller_index_port.write(0x06);
     uint8_t segment_number = ((m_graphics_controller_data_port.read() >> 2) & 0x03);
     switch (segment_number) {
          default:
          case 0: return (uint8_t*)0x00000; break;
          case 1: return (uint8_t*)0xA0000; break;
          case 2: return (uint8_t*)0xB0000; break;
          case 3: return (uint8_t*)0xB8000; break;
     }
}

uint8_t *video_graphics_array::get_frame_buffer_segment()
{
     m_graphics_controller_index_port.write(0x06);
     uint8_t segment_number = ((m_graphics_controller_data_port.read() >> 2) & 0x03);
     switch (segment_number) {
          default:
          case 0: return (uint8_t *)0x00000; break;
          case 1: return (uint8_t *)0xA0000; break;
          case 2: return (uint8_t *)0xB0000; break;
          case 3: return (uint8_t *)0xB8000; break;
     }
}

void video_graphics_array::put_pixel(int32_t m_x, int32_t m_y, uint8_t color_index)
{
     if (m_x < 0 || m_x >= 320 || m_y < 0 || m_y >= 200) {
          return;
     }
     uint8_t *pixel_address = get_frame_buffer_segment() + 320 * m_y + m_x;
     *pixel_address = color_index;
}

uint8_t video_graphics_array::get_color_index(uint32_t m_r, uint32_t m_g, uint8_t m_b)
{
     if (m_r == 0x00 && m_g == 0x00 && m_b == 0x00) return 0x00; //black
     if (m_r == 0x00 && m_g == 0x00 && m_b == 0xA8) return 0x01; //blue
     if (m_r == 0x00 && m_g == 0xA8 && m_b == 0x00) return 0x02; //green
     if (m_r == 0xA8 && m_g == 0x00 && m_b == 0x00) return 0x04; //red
     if (m_r == 0xFF && m_g == 0xFF && m_b == 0xFF) return 0x3F; //white
     return 0x00;
}

void video_graphics_array::put_pixel(int32_t m_x, int32_t m_y, uint8_t m_r, uint8_t m_g, uint8_t m_b)
{
     put_pixel(m_x, m_y, get_color_index(m_r, m_g, m_b));
}

void video_graphics_array::fill_rectangle(uint32_t m_x, uint32_t m_y, uint32_t m_w, uint32_t m_h,
     uint8_t m_r, uint8_t m_g, uint8_t m_b)
{
     for (int32_t Y = m_y; Y < m_y + m_h; Y++) {
        for (int32_t X = m_x; X < m_x + m_w; X++) {
            put_pixel(X, Y, m_r, m_g, m_b);
        }
    }
}
}
}
