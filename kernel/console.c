#include <kernel/console.h>
#include <kernel/initcall.h>
#include <hal/display.h>
#include <hal/serial.h>
#include <hal/spinlock.h>
#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <include/config.h>

static uint32_t         s_cursor_col = 0;
static uint32_t         s_cursor_row = 0;
static uint8_t          s_attr = JLOS_HAL_DISPLAY_ATTR_DEFAULT;
static uint32_t         s_cols = JLOS_CONSOLE_DEFAULT_COLS;
static uint32_t         s_rows = JLOS_CONSOLE_DEFAULT_ROWS;

static console_cell_t   s_history[JLOS_CONSOLE_HISTORY_LINES][JLOS_CONSOLE_MAX_COLS];
static uint32_t         s_history_write_idx = 0;
static uint32_t         s_history_line_count = 0;
static int32_t          s_view_scoll = 0;

static jlos_spinlock_t  s_console_lock = JLOS_SPINLOCK_INIT;

static void history_write_cell(uint32_t col, char ch, uint8_t attr)
{
    if (col >= s_cols) {
        return;
    }
    s_history[s_history_write_idx][col].ch = ch;
    s_history[s_history_write_idx][col].attr = attr;
}

static void history_newline(void)
{
    s_history_write_idx = (s_history_write_idx + 1) % JLOS_CONSOLE_HISTORY_LINES;
    if (s_history_line_count < JLOS_CONSOLE_HISTORY_LINES) {
        s_history_line_count++;
    }
    for (uint32_t i = 0; i < JLOS_CONSOLE_MAX_COLS; i++) {
        s_history[s_history_write_idx][i].ch = ' ';
        s_history[s_history_write_idx][i].attr = s_attr;
    }
}

static void console_putc_locked(char c)
{
    if (c == '\n') {
        s_cursor_col = 0;
        s_cursor_row++;
        history_newline();
#if JLOS_SERIAL_ECHO
        jlos_hal_serial_default_putc('\n');
#endif
    } else if (c == '\b') {
        if (s_cursor_col > 0) {
            s_cursor_col--;
        }
        jlos_hal_display_putc_at(s_cursor_col, s_cursor_row, ' ', s_attr);
        history_write_cell(s_cursor_col, ' ', s_attr);
    } else {
        jlos_hal_display_putc_at(s_cursor_col, s_cursor_row, c, s_attr);
        history_write_cell(s_cursor_col, c, s_attr);
#if JLOS_SERIAL_ECHO
        jlos_hal_serial_default_putc(c);
#endif
        s_cursor_col++;
    }

    if (s_cursor_col >= s_cols) {
        s_cursor_col = 0;
        s_cursor_row++;
        history_newline();
    }

    if (s_cursor_row >= s_rows) {
        jlos_hal_display_scroll_up(s_attr);
        s_cursor_row = s_rows - 1;
        s_cursor_col = 0;
    }
}

void jlos_console_init(void)
{
    jlos_spinlock_init(&s_console_lock);
    jlos_hal_display_init();

    jlos_hal_display_info_t info = {
        .cols = JLOS_CONSOLE_DEFAULT_COLS,
        .rows = JLOS_CONSOLE_DEFAULT_ROWS,
        .pixel_width = 0,
        .pixel_height = 0,
        .bpp = 0,
        .type = JLOS_HAL_DISPLAY_TYPE_TEXT,
    };
    jlos_hal_display_get_info(&info);
    s_cols = info.cols;
    s_rows = info.rows;
    if (s_cols > JLOS_CONSOLE_MAX_COLS) {
        s_cols = JLOS_CONSOLE_MAX_COLS;
    }

    s_cursor_col = 0;
    s_cursor_row = 0;
    s_attr = JLOS_HAL_DISPLAY_ATTR_DEFAULT;

    jlos_hal_display_clear(s_attr);
    jlos_hal_display_draw_cursor(s_cursor_col, s_cursor_row);
}

void jlos_console_reinit(void)
{
    jlos_hal_display_info_t info = {
        .cols = JLOS_CONSOLE_DEFAULT_COLS,
        .rows = JLOS_CONSOLE_DEFAULT_ROWS,
        .pixel_width = 0,
        .pixel_height = 0,
        .bpp = 0,
        .type = JLOS_HAL_DISPLAY_TYPE_TEXT,
    };
    jlos_hal_display_get_info(&info);

    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    s_cols = info.cols;
    s_rows = info.rows;
    if (s_cols > JLOS_CONSOLE_MAX_COLS) {
        s_cols = JLOS_CONSOLE_MAX_COLS;
    }
    
    s_cursor_col = 0;
    s_cursor_row = 0;
    jlos_hal_display_draw_cursor(0, 0);
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

static void console_render_view(void)
{
    int32_t eff_count = (int32_t)s_history_line_count + 1;
    int32_t latest = eff_count - 1;
    int32_t start = latest - s_view_scoll - (int32_t)s_rows + 1;
    if (start < 0) {
        start = 0;
    }
    uint32_t base_idx =
        (s_history_write_idx + JLOS_CONSOLE_HISTORY_LINES - (int32_t)s_history_line_count + start) % JLOS_CONSOLE_HISTORY_LINES;
    for (uint32_t row = 0; row < s_rows; row++) {
        int32_t hist_row = start + (int32_t)row;
        if (hist_row >= eff_count) {
            jlos_hal_display_putc_at(0, row, ' ', s_attr);
            continue;
        }
        uint32_t idx = (base_idx + row) % JLOS_CONSOLE_HISTORY_LINES;
        for (uint32_t col = 0; col < s_cols; col++) {
            jlos_hal_display_putc_at(col, row, s_history[idx][col].ch, s_history[idx][col].attr);
        }
    }
}

void jlos_console_putc(char c)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    if (s_view_scoll > 0) {
        s_view_scoll = 0;
        console_render_view();
    }
    jlos_hal_display_erase_cursor(s_cursor_col, s_cursor_row);
    console_putc_locked(c);
    jlos_hal_display_draw_cursor(s_cursor_col, s_cursor_row);
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

void jlos_console_puts(const char *str)
{
    if (!str) {
        return;
    }
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    if (s_view_scoll > 0) {
        s_view_scoll = 0;
        console_render_view();
    }
    jlos_hal_display_erase_cursor(s_cursor_col, s_cursor_row);
    for (int i = 0; str[i] != '\0'; i++) {
        console_putc_locked(str[i]);
    }
    jlos_hal_display_draw_cursor(s_cursor_col, s_cursor_row);
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

void jlos_console_clear(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    jlos_hal_display_clear(s_attr);
    s_cursor_col = 0;
    s_cursor_row = 0;
    jlos_hal_display_draw_cursor(s_cursor_col, s_cursor_row);
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

void jlos_console_set_attr(uint8_t attr)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    s_attr = attr;
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

uint8_t jlos_console_get_attr(void)
{
    return s_attr;
}

void jlos_console_get_cursor(uint32_t *col, uint32_t *row)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    if (col) {
        *col = s_cursor_col;
    }
    if (row) {
        *row = s_cursor_row;
    }
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

void jlos_console_set_cursor(uint32_t col, uint32_t row)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    jlos_hal_display_erase_cursor(s_cursor_col, s_cursor_row);
    if (col < s_cols) {
        s_cursor_col = col;
    }
    if (row < s_rows) {
        s_cursor_row = row;
    }
    jlos_hal_display_draw_cursor(s_cursor_col, s_cursor_row);
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

uint32_t jlos_console_get_cols(void)
{
    return s_cols;
}

uint32_t jlos_console_get_rows(void)
{
    return s_rows;
}

void jlos_console_lock(uint32_t *flags)
{
    *flags = jlos_spin_lock_irqsave(&s_console_lock);
}

void jlos_console_unlock(uint32_t flags)
{
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

static void display_device_init(void)
{
    jlos_hal_arch_display_init_fb();
    jlos_console_reinit();
}

void jlos_console_scroll_view(int32_t delta)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    jlos_hal_display_erase_cursor(s_cursor_col, s_cursor_row);
    s_view_scoll += delta;
    if (s_view_scoll < 0) {
        s_view_scoll = 0;
    }
    int32_t max_scroll = (int32_t)s_history_line_count + 1 - (int32_t)s_rows;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    if (s_view_scoll > max_scroll) {
        s_view_scoll = max_scroll;
    }
    if (s_view_scoll == 0) {
        console_render_view();
        jlos_hal_display_draw_cursor(s_cursor_col, s_cursor_row);
    } else {
        console_render_view();
    }
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

void jlos_console_reset_view(void)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_console_lock);
    s_view_scoll = 0;
    console_render_view();
    jlos_hal_display_draw_cursor(s_cursor_col, s_cursor_row);
    jlos_spin_unlock_irqrestore(&s_console_lock, flags);
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, display_device_init);
