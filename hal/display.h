#ifndef _JLOS_HAL_DISPLAY_H
#define _JLOS_HAL_DISPLAY_H

#include <common/types.h>

typedef enum {
    JLOS_HAL_DISPLAY_TYPE_TEXT        = 0,
    JLOS_HAL_DISPLAY_TYPE_FRAMEBUFFER = 1,
} jlos_hal_display_type_t;

typedef struct {
    uint32_t cols;
    uint32_t rows;
    uint32_t pixel_width;
    uint32_t pixel_height;
    uint8_t  bpp;
    jlos_hal_display_type_t type;
} jlos_hal_display_info_t;

typedef struct {
    void (*init)(void);
    void (*putc_at)(uint32_t col, uint32_t row, char c, uint8_t attr);
    void (*clear)(uint8_t attr);
    void (*scroll_up)(uint8_t attr);
    void (*erase_cursor)(uint32_t col, uint32_t row);
    void (*draw_cursor)(uint32_t col, uint32_t row);
    void (*get_info)(jlos_hal_display_info_t *info);
    void (*invert_at)(uint32_t col, uint32_t row);
} jlos_hal_display_ops_t;

#define JLOS_HAL_DISPLAY_ATTR_FG_WHITE_BG_BLACK  0x07
#define JLOS_HAL_DISPLAY_ATTR_DEFAULT            JLOS_HAL_DISPLAY_ATTR_FG_WHITE_BG_BLACK

extern const jlos_hal_display_ops_t *jlos_hal_display_ops;

void jlos_hal_display_init(void);
void jlos_hal_display_putc_at(uint32_t col, uint32_t row, char c, uint8_t attr);
void jlos_hal_display_clear(uint8_t attr);
void jlos_hal_display_scroll_up(uint8_t attr);
void jlos_hal_display_erase_cursor(uint32_t col, uint32_t row);
void jlos_hal_display_draw_cursor(uint32_t col, uint32_t row);
void jlos_hal_display_get_info(jlos_hal_display_info_t *info);

void jlos_hal_display_invert_at(uint32_t col, uint32_t row);

#endif
