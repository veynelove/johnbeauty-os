#include <hal/display.h>
#include <hal/io.h>
#include <kernel/paging.h>

#define JLOS_VGA_TEXT_COLS        80
#define JLOS_VGA_TEXT_ROWS        25
#define JLOS_VGA_TEXT_LAST_ROW    24
#define JLOS_VGA_TEXT_BUFFER_PA   0xB8000
#define JLOS_VGA_TEXT_BUFFER_VA   ((uint16_t *)PHYS_TO_VIRT(JLOS_VGA_TEXT_BUFFER_PA))

#define JLOS_VGA_CRT_INDEX_PORT   0x3D4
#define JLOS_VGA_CRT_DATA_PORT    0x3D5
#define JLOS_VGA_CRT_CURSOR_HIGH  0x0E
#define JLOS_VGA_CRT_CURSOR_LOW   0x0F
#define JLOS_VGA_CRT_CURSOR_MAX   (JLOS_VGA_TEXT_COLS * JLOS_VGA_TEXT_ROWS)

static void vga_text_clear(uint8_t attr);
static void vga_text_draw_cursor(uint32_t col, uint32_t row);

static void vga_text_init(void)
{
    vga_text_clear(JLOS_HAL_DISPLAY_ATTR_DEFAULT);
    vga_text_draw_cursor(0, 0);
}

static void vga_text_putc_at(uint32_t col, uint32_t row, char c, uint8_t attr)
{
    if (col >= JLOS_VGA_TEXT_COLS || row >= JLOS_VGA_TEXT_ROWS) return;
    JLOS_VGA_TEXT_BUFFER_VA[row * JLOS_VGA_TEXT_COLS + col] = ((uint16_t)attr << 8) | (uint8_t)c;
}

static void vga_text_clear(uint8_t attr)
{
    uint16_t fill = ((uint16_t)attr << 8) | ' ';
    for (uint32_t i = 0; i < JLOS_VGA_TEXT_COLS * JLOS_VGA_TEXT_ROWS; i++)
        JLOS_VGA_TEXT_BUFFER_VA[i] = fill;
}

static void vga_text_scroll_up(uint8_t attr)
{
    uint16_t fill = ((uint16_t)attr << 8) | ' ';
    for (uint32_t row = 0; row < JLOS_VGA_TEXT_LAST_ROW; row++) {
        for (uint32_t col = 0; col < JLOS_VGA_TEXT_COLS; col++) {
            JLOS_VGA_TEXT_BUFFER_VA[row * JLOS_VGA_TEXT_COLS + col] =
                JLOS_VGA_TEXT_BUFFER_VA[(row + 1) * JLOS_VGA_TEXT_COLS + col];
        }
    }
    for (uint32_t col = 0; col < JLOS_VGA_TEXT_COLS; col++)
        JLOS_VGA_TEXT_BUFFER_VA[JLOS_VGA_TEXT_LAST_ROW * JLOS_VGA_TEXT_COLS + col] = fill;
}

static void vga_text_erase_cursor(uint32_t col, uint32_t row)
{
    (void)col;
    (void)row;
}

static void vga_text_draw_cursor(uint32_t col, uint32_t row)
{
    uint16_t pos = (uint16_t)(row * JLOS_VGA_TEXT_COLS + col);
    if (pos >= JLOS_VGA_CRT_CURSOR_MAX) pos = 0;

    jlos_io8_t idx, dat;
    jlos_io8_init(&idx, JLOS_VGA_CRT_INDEX_PORT);
    jlos_io8_init(&dat, JLOS_VGA_CRT_DATA_PORT);

    jlos_io8_write(&idx, JLOS_VGA_CRT_CURSOR_LOW);
    jlos_io8_write(&dat, (uint8_t)(pos & 0xFF));
    jlos_io8_write(&idx, JLOS_VGA_CRT_CURSOR_HIGH);
    jlos_io8_write(&dat, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_text_get_info(jlos_hal_display_info_t *info)
{
    info->cols         = JLOS_VGA_TEXT_COLS;
    info->rows         = JLOS_VGA_TEXT_ROWS;
    info->pixel_width  = 0;
    info->pixel_height = 0;
    info->bpp          = 0;
    info->type         = JLOS_HAL_DISPLAY_TYPE_TEXT;
}

static void vga_text_invert_at(uint32_t col, uint32_t row)
{
    if (col >= JLOS_VGA_TEXT_COLS || row >= JLOS_VGA_TEXT_ROWS) {
        return;
    }
    uint16_t *cell = &JLOS_VGA_TEXT_BUFFER_VA[row * JLOS_VGA_TEXT_COLS + col];
    uint8_t attr = (uint8_t)(*cell >> 8);
    uint8_t new_attr = ((attr & 0x0F) << 4) | ((attr & 0xF0) >> 4);
    *cell = ((uint16_t)new_attr << 8) | (*cell & 0xFF);
}

const jlos_hal_display_ops_t jlos_hal_x86_vga_text_ops = {
    .init          = vga_text_init,
    .putc_at       = vga_text_putc_at,
    .clear         = vga_text_clear,
    .scroll_up     = vga_text_scroll_up,
    .erase_cursor = vga_text_erase_cursor,
    .draw_cursor   = vga_text_draw_cursor,
    .get_info      = vga_text_get_info,
    .invert_at     = vga_text_invert_at,
};
