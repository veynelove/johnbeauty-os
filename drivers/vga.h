#ifndef __DRIVERS_VGA_H
#define __DRIVERS_VGA_H

#include <common/types.h>
#include <hal/io.h>
#include <drivers/driver.h>

typedef struct jlos_vga jlos_vga_t;

struct jlos_vga {
    jlos_io8_t m_misc_port;
    jlos_io8_t m_crtc_index_port;
    jlos_io8_t m_crtc_data_port;
    jlos_io8_t m_sequencer_index_port;
    jlos_io8_t m_sequencer_data_port;
    jlos_io8_t m_graphics_controller_index_port;
    jlos_io8_t m_graphics_controller_data_port;
    jlos_io8_t m_attribute_controller_index_port;
    jlos_io8_t m_attribute_controller_read_port;
    jlos_io8_t m_attribite_controller_write_port;
    jlos_io8_t m_attribite_controller_reset_port;
};

void jlos_vga_init(jlos_vga_t* self);
void jlos_vga_destroy(jlos_vga_t* self);

bool jlos_vga_support_mode(jlos_vga_t* self, uint32_t width, uint32_t height, uint32_t colordepth);
bool jlos_vga_set_mode(jlos_vga_t* self, uint32_t width, uint32_t height, uint32_t colordepth);
void jlos_vga_put_pixel(jlos_vga_t* self, int32_t m_x, int32_t m_y, uint8_t m_r, uint8_t m_g, uint8_t m_b);
void jlos_vga_put_pixel_color(jlos_vga_t* self, int32_t m_x, int32_t m_y, uint8_t color_index);
void jlos_vga_fill_rectangle(jlos_vga_t* self, uint32_t m_x, uint32_t m_y, uint32_t m_w, uint32_t m_h, uint8_t m_r, uint8_t m_g, uint8_t m_b);

#endif