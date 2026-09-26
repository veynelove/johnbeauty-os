#include <hal/display.h>
#include <hal/paging.h>
#include <common/multiboot.h>
#include <kernel/paging.h>
#include <kernel/memory_manager.h>
#include <kernel/device.h>
#include <arch/x86/font_8x16.h>

#define JLOS_FB_BPP              32
#define JLOS_FB_CURSOR_HEIGHT    2

static uint32_t *s_fb_base = NULL;
static uint32_t s_fb_pitch = 0;
static uint32_t s_fb_pixel_width = 0;
static uint32_t s_fb_pixel_height = 0;
static uint32_t s_fb_cols = 0;
static uint32_t s_fb_rows = 0;

static const uint32_t jlos_fb_palette[16] = {
    0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
    0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
    0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
    0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF,
};

static void fb_putc_at(uint32_t col, uint32_t row, char c, uint8_t attr)
{
    uint32_t fg = jlos_fb_palette[attr & 0x0F];
    uint32_t bg = jlos_fb_palette[(attr >> 4) & 0x0F];
    const uint8_t *glyph = jlos_font_8x16[(uint8_t)c];
    uint32_t px = col * JLOS_FONT_GLYPH_WIDTH;
    uint32_t py = row * JLOS_FONT_GLYPH_HEIGHT;
    uint32_t row_pixels = s_fb_pitch / (JLOS_FB_BPP / 8);

    for (uint32_t y = 0; y < JLOS_FONT_GLYPH_HEIGHT; y++) {
        uint8_t bits = glyph[y];
        for (uint32_t x = 0; x < JLOS_FONT_GLYPH_WIDTH; x++) {
            s_fb_base[(py + y) * row_pixels + (px + x)] =
                (bits & (0x80 >> x)) ? fg : bg;
        }
    }
}

static void fb_clear(uint8_t attr)
{
    uint32_t bg = jlos_fb_palette[(attr >> 4) & 0x0F];
    uint32_t total = s_fb_pixel_height * (s_fb_pitch / (JLOS_FB_BPP / 8));
    for (uint32_t i = 0; i < total; i++)
        s_fb_base[i] = bg;
}

static void fb_scroll_up(uint8_t attr)
{
    uint32_t bg = jlos_fb_palette[(attr >> 4) & 0x0F];
    uint32_t row_pixels = s_fb_pitch / (JLOS_FB_BPP / 8);
    uint32_t scroll_pixels = JLOS_FONT_GLYPH_HEIGHT * row_pixels;
    uint32_t total_pixels = s_fb_pixel_height * row_pixels;
    uint32_t move_pixels = total_pixels - scroll_pixels;

    jlos_memcpy(s_fb_base, s_fb_base + scroll_pixels, move_pixels * (JLOS_FB_BPP / 8));

    for (uint32_t i = 0; i < scroll_pixels; i++)
        s_fb_base[move_pixels + i] = bg;
}

static void fb_invert_at(uint32_t col, uint32_t row);

static void fb_erase_cursor(uint32_t col, uint32_t row)
{
    uint32_t px = col * JLOS_FONT_GLYPH_WIDTH;
    uint32_t py = row * JLOS_FONT_GLYPH_HEIGHT;
    uint32_t row_pixels = s_fb_pitch / (JLOS_FB_BPP / 8);
    uint32_t y_start = py + JLOS_FONT_GLYPH_HEIGHT - JLOS_FB_CURSOR_HEIGHT;

    for (uint32_t y = y_start; y < py + JLOS_FONT_GLYPH_HEIGHT; y++) {
        for (uint32_t x = 0; x < JLOS_FONT_GLYPH_WIDTH; x++) {
            uint32_t *pix = &s_fb_base[y * row_pixels + (px + x)];
            *pix = ~*pix;
        }
    }
}

static void fb_draw_cursor(uint32_t col, uint32_t row)
{
    uint32_t px = col * JLOS_FONT_GLYPH_WIDTH;
    uint32_t py = row * JLOS_FONT_GLYPH_HEIGHT;
    uint32_t row_pixels = s_fb_pitch / (JLOS_FB_BPP / 8);
    uint32_t y_start = py + JLOS_FONT_GLYPH_HEIGHT - JLOS_FB_CURSOR_HEIGHT;

    for (uint32_t y = y_start; y < py + JLOS_FONT_GLYPH_HEIGHT; y++) {
        for (uint32_t x = 0; x < JLOS_FONT_GLYPH_WIDTH; x++) {
            uint32_t *pix = &s_fb_base[y * row_pixels + (px + x)];
            *pix = ~*pix;
        }
    }
}

static void fb_invert_at(uint32_t col, uint32_t row)
{
    uint32_t px = col * JLOS_FONT_GLYPH_WIDTH;
    uint32_t py = row * JLOS_FONT_GLYPH_HEIGHT;
    uint32_t row_pixels = s_fb_pitch / (JLOS_FB_BPP / 8);

    for (uint32_t y = 0; y < JLOS_FONT_GLYPH_HEIGHT; y++) {
        for (uint32_t x = 0; x < JLOS_FONT_GLYPH_WIDTH; x++) {
            uint32_t *pix = &s_fb_base[(py + y) * row_pixels + (px + x)];
            *pix = ~*pix;
        }
    }
}

static void fb_get_info(jlos_hal_display_info_t *info)
{
    info->cols         = s_fb_cols;
    info->rows         = s_fb_rows;
    info->pixel_width  = s_fb_pixel_width;
    info->pixel_height = s_fb_pixel_height;
    info->bpp          = JLOS_FB_BPP;
    info->type         = JLOS_HAL_DISPLAY_TYPE_FRAMEBUFFER;
}

static const jlos_hal_display_ops_t jlos_hal_x86_fb_ops = {
    .init          = NULL,
    .putc_at       = fb_putc_at,
    .clear         = fb_clear,
    .scroll_up     = fb_scroll_up,
    .erase_cursor = fb_erase_cursor,
    .draw_cursor   = fb_draw_cursor,
    .get_info      = fb_get_info,
    .invert_at     = fb_invert_at,
};

void jlos_hal_arch_display_init_fb(void)
{
    const multiboot_info_t *mb = jlos_device_multiboot_info;
    jlos_vbe_mode_info_t *vbe = (jlos_vbe_mode_info_t *)PHYS_TO_VIRT(mb->vbe_mode_info);

    uint32_t fb_phys = vbe->phys_base_ptr;
    uint32_t fb_size = vbe->bytes_per_scanline * vbe->y_resolution;

    uint32_t fb_virt = KERNEL_VIRTUAL_BASE + jlos_device_physical_memory_end;
    fb_virt = JLOS_ALIGN_UP(fb_virt, 4 * 1024 * 1024);

    jlos_paging_map_range(jlos_hal_paging_get_active_context(), fb_virt, fb_phys, fb_size, JLOS_PG_KERNEL_RW);

    s_fb_base         = (uint32_t *)fb_virt;
    s_fb_pitch        = vbe->bytes_per_scanline;
    s_fb_pixel_width  = vbe->x_resolution;
    s_fb_pixel_height = vbe->y_resolution;
    s_fb_cols         = vbe->x_resolution / JLOS_FONT_GLYPH_WIDTH;
    s_fb_rows         = vbe->y_resolution / JLOS_FONT_GLYPH_HEIGHT;

    jlos_hal_display_ops = &jlos_hal_x86_fb_ops;
    fb_clear(0x07);
}
