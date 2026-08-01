#ifndef __DRIVERS_VGA_H
#define __DRIVERS_VGA_H

#include <common/types.h>
#include <hal/io.h>
#include <drivers/driver.h>

typedef struct jlos_vga jlos_vga_t;

struct jlos_vga {
    jlos_io8_t misc_port;
    jlos_io8_t crtc_index_port;
    jlos_io8_t crtc_data_port;
    jlos_io8_t sequencer_index_port;
    jlos_io8_t sequencer_data_port;
    jlos_io8_t graphics_controller_index_port;
    jlos_io8_t graphics_controller_data_port;
    jlos_io8_t attribute_controller_index_port;
    jlos_io8_t attribute_controller_read_port;
    jlos_io8_t attribite_controller_write_port;
    jlos_io8_t attribite_controller_reset_port;
};

void jlos_vga_init(jlos_vga_t* self);
void jlos_vga_destroy(jlos_vga_t* self);

bool jlos_vga_support_mode(jlos_vga_t* self, uint32_t width, uint32_t height, uint32_t colordepth);
bool jlos_vga_set_mode(jlos_vga_t* self, uint32_t width, uint32_t height, uint32_t colordepth);
void jlos_vga_put_pixel(jlos_vga_t* self, int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b);
void jlos_vga_put_pixel_color(jlos_vga_t* self, int32_t x, int32_t y, uint8_t color_index);
void jlos_vga_fill_rectangle(jlos_vga_t* self, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b);

#endif
