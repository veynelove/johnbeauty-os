#include <drivers/vga.h>
#include <kernel/paging.h>

#define VGA_FB_VA(pa)  PHYS_TO_VIRT(pa)

static void jlos_vga_write_registers(jlos_vga_t* self, uint8_t *registers);
static uint8_t *jlos_vga_get_frame_buffer_segment(jlos_vga_t* self);
static uint8_t jlos_vga_get_color_index(uint32_t r, uint32_t g, uint8_t b);

void jlos_vga_init(jlos_vga_t* self)
{
    jlos_io8_init(&self->misc_port, 0x3c2);
    jlos_io8_init(&self->crtc_index_port, 0x3d4);
    jlos_io8_init(&self->crtc_data_port, 0x3d5);
    jlos_io8_init(&self->sequencer_index_port, 0x3c4);
    jlos_io8_init(&self->sequencer_data_port, 0x3c5);
    jlos_io8_init(&self->graphics_controller_index_port, 0x3ce);
    jlos_io8_init(&self->graphics_controller_data_port, 0x3cf);
    jlos_io8_init(&self->attribute_controller_index_port, 0x3c0);
    jlos_io8_init(&self->attribute_controller_read_port, 0x3c1);
    jlos_io8_init(&self->attribite_controller_write_port, 0x3c0);
    jlos_io8_init(&self->attribite_controller_reset_port, 0x3da);
}

void jlos_vga_destroy(jlos_vga_t* self)
{
    (void)self;
}

static void jlos_vga_write_registers(jlos_vga_t* self, uint8_t *registers)
{
    jlos_io8_write(&self->misc_port, *(registers++));
    for (uint8_t i = 0; i < 5; i++) {
        jlos_io8_write(&self->sequencer_index_port, i);
        jlos_io8_write(&self->sequencer_data_port, *(registers++));
    }
    jlos_io8_write(&self->crtc_index_port, 0x03);
    jlos_io8_write(&self->crtc_data_port, jlos_io8_read(&self->crtc_data_port) | 0x80);
    jlos_io8_write(&self->crtc_index_port, 0x11);
    jlos_io8_write(&self->crtc_data_port, jlos_io8_read(&self->crtc_data_port) & ~0x80);

    registers[0x03] = registers[0x03] | 0x80;
    registers[0x11] = registers[0x11] & ~0x80;

    for (uint8_t i = 0; i < 25; i++) {
        jlos_io8_write(&self->crtc_index_port, i);
        jlos_io8_write(&self->crtc_data_port, *(registers++));
    }
    for (uint8_t i = 0; i < 9; i++) {
        jlos_io8_write(&self->graphics_controller_index_port, i);
        jlos_io8_write(&self->graphics_controller_data_port, *(registers++));
    }
    for (uint8_t i = 0; i < 21; i++) {
        jlos_io8_read(&self->attribite_controller_reset_port);
        jlos_io8_write(&self->attribute_controller_index_port, i);
        jlos_io8_write(&self->attribite_controller_write_port, *(registers++));
    }
    jlos_io8_read(&self->attribite_controller_reset_port);
    jlos_io8_write(&self->attribute_controller_index_port, 0x20);
}

bool jlos_vga_support_mode(jlos_vga_t* self, uint32_t width, uint32_t height, uint32_t colordepth)
{
    (void)self;
    return width == 320 && height == 200 && colordepth == 8;
}

bool jlos_vga_set_mode(jlos_vga_t* self, uint32_t width, uint32_t height, uint32_t colordepth)
{
    if (!jlos_vga_support_mode(self, width, height, colordepth)) {
        return false;
    }
    uint8_t g_320x200x256[] = {
        0x63,
        0x03, 0x01, 0x0F, 0x00, 0x0E,
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
        0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
        0xFF,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41, 0x00, 0x0F, 0x00, 0x00
    };
    jlos_vga_write_registers(self, g_320x200x256);
    return true;
}

static uint8_t *jlos_vga_get_frame_buffer_segment(jlos_vga_t* self)
{
    jlos_io8_write(&self->graphics_controller_index_port, 0x06);
    uint8_t segment_number = ((jlos_io8_read(&self->graphics_controller_data_port) >> 2) & 0x03);
    switch (segment_number) {
        default:
        case 0: return (uint8_t*)VGA_FB_VA(0x00000);
        case 1: return (uint8_t*)VGA_FB_VA(0xA0000);
        case 2: return (uint8_t*)VGA_FB_VA(0xB0000);
        case 3: return (uint8_t*)VGA_FB_VA(0xB8000);
    }
}

static uint8_t jlos_vga_get_color_index(uint32_t r, uint32_t g, uint8_t b)
{
    if (r == 0x00 && g == 0x00 && b == 0x00) return 0x00;
    if (r == 0x00 && g == 0x00 && b == 0xA8) return 0x01;
    if (r == 0x00 && g == 0xA8 && b == 0x00) return 0x02;
    if (r == 0xA8 && g == 0x00 && b == 0x00) return 0x04;
    if (r == 0xFF && g == 0xFF && b == 0xFF) return 0x3F;
    return 0x00;
}

void jlos_vga_put_pixel_color(jlos_vga_t* self, int32_t x, int32_t y, uint8_t color_index)
{
    if (x < 0 || x >= 320 || y < 0 || y >= 200) {
        return;
    }
    uint8_t *pixel_address = jlos_vga_get_frame_buffer_segment(self) + 320 * y + x;
    *pixel_address = color_index;
}

void jlos_vga_put_pixel(jlos_vga_t* self, int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    jlos_vga_put_pixel_color(self, x, y, jlos_vga_get_color_index(r, g, b));
}

void jlos_vga_fill_rectangle(jlos_vga_t* self, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
    uint8_t r, uint8_t g, uint8_t b)
{
    for (int32_t Y = y; Y < (int32_t)(y + h); Y++) {
        for (int32_t X = x; X < (int32_t)(x + w); X++) {
            jlos_vga_put_pixel(self, X, Y, r, g, b);
        }
    }
}
