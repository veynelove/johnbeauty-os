#include <kernel/printk.h>
#include <kernel/console.h>
#include <hal/serial.h>
#include <hal/timer.h>

void jlos_printk_init(void)
{
    jlos_hal_serial_default_init();
    jlos_console_init();
}

void printf(const char *str)
{
    jlos_console_puts(str);
}

void printf_hex(uint8_t key)
{
    char foo[3] = "00";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0F];
    foo[1] = hex[key & 0x0F];
    jlos_console_puts(foo);
}

void printf_hex16(uint16_t value)
{
    char foo[5] = "0000";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(value >> 12) & 0x0F];
    foo[1] = hex[(value >> 8) & 0x0F];
    foo[2] = hex[(value >> 4) & 0x0F];
    foo[3] = hex[value & 0x0F];
    jlos_console_puts(foo);
}

void printf_hex32(uint32_t value)
{
    char foo[9] = "00000000";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(value >> 28) & 0x0F];
    foo[1] = hex[(value >> 24) & 0x0F];
    foo[2] = hex[(value >> 20) & 0x0F];
    foo[3] = hex[(value >> 16) & 0x0F];
    foo[4] = hex[(value >> 12) & 0x0F];
    foo[5] = hex[(value >> 8) & 0x0F];
    foo[6] = hex[(value >> 4) & 0x0F];
    foo[7] = hex[value & 0x0F];
    jlos_console_puts(foo);
}

void printf_char(char c)
{
    jlos_console_putc(c);
}

static void printk_itoa(int value, int base)
{
    char buffer[32];
    char *digits = "0123456789ABCDEF";
    int i = 0;
    bool negative = false;
    if (value == 0) {
        jlos_console_putc('0');
        return;
    }
    if (value < 0 && base == 10) {
        negative = true;
        value = -value;
    }
    while (value > 0) {
        buffer[i++] = digits[value % base];
        value /= base;
    }
    if (negative) {
        jlos_console_putc('-');
    }
    while (i > 0) {
        jlos_console_putc(buffer[--i]);
    }
}

static void printk_utoa(unsigned int value, int base, bool uppercase)
{
    char buffer[32];
    char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    
    if (value == 0) {
        jlos_console_putc('0');
        return;
    }
    
    while (value > 0) {
        buffer[i++] = digits[value % base];
        value /= base;
    }
    
    while (i > 0) {
        jlos_console_putc(buffer[--i]);
    }
}

static void printk_put_us_padded(unsigned int value, int width)
{
    char buf[16];
    const char *digits = "0123456789";
    int i = 0;
    do { buf[i++] = digits[value % 10]; value /= 10; } while (value > 0);
    while (i < width) buf[i++] = '0';
    while (i > 0) jlos_console_putc(buf[--i]);
}

#if JLOS_KERNEL_LOG_PRINT_LEVEL
static char printk_level_char(int level)
{
    switch (level) {
    case JLOS_KERNEL_LOG_EMERG:  return 'P';  /* panic */
    case JLOS_KERNEL_LOG_ALERT:  return 'A';
    case JLOS_KERNEL_LOG_CRIT:   return 'C';
    case JLOS_KERNEL_LOG_ERR:    return 'E';
    case JLOS_KERNEL_LOG_WARN:   return 'W';
    case JLOS_KERNEL_LOG_NOTICE: return 'N';
    case JLOS_KERNEL_LOG_INFO:   return 'I';
    case JLOS_KERNEL_LOG_DEBUG:  return 'D';
    default:                     return '?';
    }
}
#endif

static void printk_print_prefix(uint32_t ticks, int level, const char *subsys, const char *func)
{
    uint32_t sec = ticks / JLOS_HAL_TIME_FREQ_HZ;
    uint32_t us  = (ticks % JLOS_HAL_TIME_FREQ_HZ) * (1000000 / JLOS_HAL_TIME_FREQ_HZ);
    jlos_console_putc('[');
    printk_utoa(sec, 10, false);
    jlos_console_putc('.');
    printk_put_us_padded(us, 6);
    jlos_console_putc(']');
    jlos_console_putc(' ');
#if JLOS_KERNEL_LOG_PRINT_LEVEL
    jlos_console_putc('[');
    jlos_console_putc(printk_level_char(level));
    jlos_console_putc(']');
    jlos_console_putc(' ');
#endif
#if JLOS_KERNEL_LOG_PRINT_SUBSYS
    jlos_console_putc('[');
    printk_puts(subsys);
    jlos_console_putc(']');
    jlos_console_putc(' ');
#endif
    if (func) {
        jlos_console_putc('[');
        jlos_console_puts(func);
        jlos_console_putc(']');
        jlos_console_putc(' ');
    }
    (void)level;
    (void)subsys;
}

void printk(int level, const char *subsys, const char *func, const char *fmt, ...)
{
    uint32_t *args = (uint32_t *)&fmt + 1;
    uint32_t flags;
    jlos_console_lock(&flags);
    uint32_t ticks = jlos_hal_timer_get_ticks();
    int at_line_start = 1;

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '\n') {
            jlos_console_putc('\n');
            at_line_start = 1;
            continue;
        }
        if (at_line_start) {
            printk_print_prefix(ticks, level, subsys, func);
            at_line_start = 0;
        }
        if (fmt[i] == '%') {
            i++;
            switch(fmt[i]) {
                case 'd': {
                    int value = *args++;
                    printk_itoa(value, 10);
                    break;
                }
                case 'u': {
                    unsigned int value = *args++;
                    printk_utoa(value, 10, false);
                    break;
                }
                case 'x': {
                    unsigned int value = *args++;
                    printk_utoa(value, 16, false);
                    break;
                }
                case 'X': {
                    unsigned int value = *args++;
                    printk_utoa(value, 16, true);
                    break;
                }
                case 's': {
                    const char *str = (const char *)*args++;
                    jlos_console_puts(str);
                    break;
                }
                case 'c': {
                    char c = (char)*args++;
                    jlos_console_putc(c);
                    break;
                }
                case 'p': {
                    unsigned int value = *args++;
                    jlos_console_puts("0x");
                    printk_utoa(value, 16, false);
                    break;
                }
                case '%': {
                    jlos_console_putc('%');
                    break;
                }
                default: {
                    jlos_console_putc('%');
                    jlos_console_putc(fmt[i]);
                    break;
                }
            }
        } else {
            jlos_console_putc(fmt[i]);
        }
    }
    jlos_console_unlock(flags);
}
