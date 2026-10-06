#include <stdarg.h>
#include <hal/serial.h>
#include <hal/clock_event.h>
#include <hal/spinlock.h>
#include <kernel/printk.h>
#include <kernel/console.h>
#include <kernel/timek.h>

static int g_printk_loglevel = JLOS_KERNEL_LOG_DEBUG;
static jlos_spinlock_t s_printk_lock = JLOS_SPINLOCK_INIT;

void jlos_printk_set_loglevel(int level)
{
    g_printk_loglevel = level;
}

void jlos_printk_init(void)
{
    jlos_hal_serial_default_init();
    jlos_console_init();
}

void printf(const char *str)
{
    jlos_console_puts(str);
}

static int printk_fmt_uint(char *buf, uint64_t value, int base, bool uppercase)
{
    const char *digits0 = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (value == 0) {
        buf[i++] = '0';
        return i;
    }
    while (value > 0) {
        buf[i++] = digits0[value % base];
        value /= base;
    }
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = t;
    }
    return i;
}

static int printk_fmt_int(char *buf, int64_t value, int base)
{
    int i = 0;
    if (value < 0 && base == 10) {
        buf[i++] = '-';
        i += printk_fmt_uint(buf + i, (uint64_t)(-value), base, false);
    } else {
        i = printk_fmt_uint(buf, (uint64_t)value, base, false);
    }
    return i;
}

static void printk_put_uint(uint64_t value, int base, bool uppercase)
{
    char buf[32];
    int n = printk_fmt_uint(buf, value, base, uppercase);
    for (int i = 0; i < n; i++) {
        jlos_console_putc(buf[i]);
    }
}


static void printk_emit_padded(const char *buf, int len, int width,
                               bool left_align, bool zero_pad)
{
    if (left_align) {
        for (int i = 0; i < len; i++) {
            jlos_console_putc(buf[i]);
        }
        for (int i = len; i < width; i++) {
            jlos_console_putc(' ');
        }
    } else {
        char pad = zero_pad ? '0' : ' ';
        for (int i = len; i < width; i++) {
            jlos_console_putc(pad);
        }
        for (int i = 0; i < len; i++) {
            jlos_console_putc(buf[i]);
        }
    }
}

#if JLOS_KERNEL_LOG_PRINT_TIME || JLOS_KERNEL_LOG_REALTIME
static void printk_put_uint_padded(unsigned int value, int width)
{
    char buf[16];
    const char *digits0 = "0123456789";
    int i = 0;

    do {
        buf[i++] = digits0[value % 10];
        value /= 10;
    } while (value > 0);

    while (i < width) {
        buf[i++] = '0';
    }

    while (i > 0) {
        jlos_console_putc(buf[--i]);
    }
}
#endif

#if JLOS_KERNEL_LOG_REALTIME
static const uint16_t s_days_before_month[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static void printk_put_realtime(void)
{
    uint64_t ns = jlos_timek_get_realtime_ns();
    uint32_t unix_sec = ns / 1000000000;
    uint32_t secs_in_day = unix_sec % 86400;
    uint32_t h = secs_in_day / 3600;
    uint32_t m = (secs_in_day % 3600) / 60;
    uint32_t s = secs_in_day % 60;
    uint32_t days = unix_sec / 86400;
    uint32_t year = 1970;

    while (1) {
        uint32_t diy = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 366 : 365;
        if (days < diy) {
            break;
        }
        days -= diy;
        year++;
    }

    uint32_t leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
    uint32_t month = 1;
    while (month < 12) {
        uint32_t dim = s_days_before_month[month] + (leap && month >= 2 ? 1 : 0);
        if (days < dim) {
            break;
        }
        month++;
    }
    uint32_t day = days - s_days_before_month[month - 1] - (leap && month - 1 >= 2 ? 1 : 0) + 1;

    jlos_console_putc('[');
    printk_put_uint(year, 10, false);
    jlos_console_putc('-');
    printk_put_uint_padded(month, 2);
    jlos_console_putc('-');
    printk_put_uint_padded(day, 2);
    jlos_console_putc(' ');
    printk_put_uint_padded(h, 2);
    jlos_console_putc(':');
    printk_put_uint_padded(m, 2);
    jlos_console_putc(':');
    printk_put_uint_padded(s, 2);
    jlos_console_putc(']');
    jlos_console_putc(' ');
}
#endif

#if JLOS_KERNEL_LOG_PRINT_LEVEL
static char printk_level_char(int level)
{
    switch (level) {
    case JLOS_KERNEL_LOG_EMERG:  return 'P';
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
    if (level < 0) {
        return;
    }
#if JLOS_KERNEL_LOG_REALTIME
    printk_put_realtime();
    (void)ticks;
#elif JLOS_KERNEL_LOG_PRINT_TIME
    uint32_t sec = ticks / JLOS_HAL_TIME_FREQ_HZ;
    uint32_t us  = (ticks % JLOS_HAL_TIME_FREQ_HZ) * (1000000 / JLOS_HAL_TIME_FREQ_HZ);
    jlos_console_putc('[');
    printk_put_uint(sec, 10, false);
    jlos_console_putc('.');
    printk_put_uint_padded(us, 6);
    jlos_console_putc(']');
    jlos_console_putc(' ');
#else
    (void)ticks;
#endif
#if JLOS_KERNEL_LOG_PRINT_LEVEL
    jlos_console_putc('[');
    jlos_console_putc(printk_level_char(level));
    jlos_console_putc(']');
    jlos_console_putc(' ');
#endif
#if JLOS_KERNEL_LOG_PRINT_SUBSYS
    jlos_console_putc('[');
    jlos_console_puts(subsys);
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
    if (level > g_printk_loglevel && level >= 0) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);

    uint32_t flags = jlos_spin_lock_irqsave(&s_printk_lock);
    uint32_t ticks = jlos_timek_get_ticks();
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
            bool left_align = false;
            bool zero_pad = false;
            int width = 0;
            while (fmt[i] == '-' || fmt[i] == '0') {
                if (fmt[i] == '-') {
                    left_align = true;
                }
                if (fmt[i] == '0') {
                    zero_pad = true;
                }
                i++;
            }
            while (fmt[i] >= '0' && fmt[i] <= '9') {
                width = width * 10 + (fmt[i] - '0');
                i++;
            }
            bool is_long_long = false;
            bool is_long = false;
            if (fmt[i] == 'l' && fmt[i + 1] == 'l') {
                is_long_long = true;
                i += 2;
            } else if (fmt[i] == 'l') {
                is_long = true;
                i += 1;
            } else if (fmt[i] == 'z') {
                is_long = true;
                i += 1;
            }
            switch (fmt[i]) {
                case 'd': {
                    int64_t value;
                    if (is_long_long) {
                        value = va_arg(ap, int64_t);
                    } else if (is_long) {
                        value = va_arg(ap, long);
                    } else {
                        value = va_arg(ap, int);
                    }
                    char buf[32];
                    int n = printk_fmt_int(buf, value, 10);
                    printk_emit_padded(buf, n, width, left_align, zero_pad);
                    break;
                }
                case 'u': {
                    uint64_t value;
                    if (is_long_long) {
                        value = va_arg(ap, uint64_t);
                    } else if (is_long) {
                        value = va_arg(ap, unsigned long);
                    } else {
                        value = va_arg(ap, unsigned int);
                    }
                    char buf[32];
                    int n = printk_fmt_uint(buf, value, 10, false);
                    printk_emit_padded(buf, n, width, left_align, zero_pad);
                    break;
                }
                case 'x': {
                    uint64_t value;
                    if (is_long_long) {
                        value = va_arg(ap, uint64_t);
                    } else if (is_long) {
                        value = va_arg(ap, unsigned long);
                    } else {
                        value = va_arg(ap, unsigned int);
                    }
                    char buf[32];
                    int n = printk_fmt_uint(buf, value, 16, false);
                    printk_emit_padded(buf, n, width, left_align, zero_pad);
                    break;
                }
                case 'X': {
                    uint64_t value;
                    if (is_long_long) {
                        value = va_arg(ap, uint64_t);
                    } else if (is_long) {
                        value = va_arg(ap, unsigned long);
                    } else {
                        value = va_arg(ap, unsigned int);
                    }
                    char buf[32];
                    int n = printk_fmt_uint(buf, value, 16, true);
                    printk_emit_padded(buf, n, width, left_align, zero_pad);
                    break;
                }
                case 'p': {
                    unsigned int value = va_arg(ap, unsigned int);
                    char buf[32];
                    buf[0] = '0';
                    buf[1] = 'x';
                    int n = 2 + printk_fmt_uint(buf + 2, value, 16, false);
                    printk_emit_padded(buf, n, width, left_align, zero_pad);
                    break;
                }
                case 's': {
                    const char *str = va_arg(ap, const char *);
                    int len = 0;
                    while (str[len]) {
                        len++;
                    }
                    printk_emit_padded(str, len, width, left_align, false);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(ap, int);
                    jlos_console_putc(c);
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
    jlos_spin_unlock_irqrestore(&s_printk_lock, flags);
    va_end(ap);
}
