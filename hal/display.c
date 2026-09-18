#include <hal/display.h>

const jlos_hal_display_ops_t *jlos_hal_display_ops = NULL;

void jlos_hal_display_init(void)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->init) {
        jlos_hal_display_ops->init();
    }
}

void jlos_hal_display_putc_at(uint32_t col, uint32_t row, char c, uint8_t attr)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->putc_at) {
        jlos_hal_display_ops->putc_at(col, row, c, attr);
    }
}

void jlos_hal_display_clear(uint8_t attr)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->clear) {
        jlos_hal_display_ops->clear(attr);
    }
}

void jlos_hal_display_scroll_up(uint8_t attr)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->scroll_up) {
        jlos_hal_display_ops->scroll_up(attr);
    }
}

void jlos_hal_display_erase_cursor(uint32_t col, uint32_t row)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->erase_cursor) {
        jlos_hal_display_ops->erase_cursor(col, row);
    }
}

void jlos_hal_display_draw_cursor(uint32_t col, uint32_t row)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->draw_cursor) {
        jlos_hal_display_ops->draw_cursor(col, row);
    }
}

void jlos_hal_display_get_info(jlos_hal_display_info_t *info)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->get_info) {
        jlos_hal_display_ops->get_info(info);
    }
}

void jlos_hal_display_invert_at(uint32_t col, uint32_t row)
{
    if (jlos_hal_display_ops && jlos_hal_display_ops->invert_at) {
        jlos_hal_display_ops->invert_at(col, row);
    }
}
