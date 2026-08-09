#include <kernel/printk.h>
#include <kernel/paging.h>
#include <hal/spinlock.h>
#include <hal/serial.h>

#define JLOS_VGA_TEXT_BUFFER_VA  ((uint16_t *)PHYS_TO_VIRT(0xB8000))

static jlos_spinlock_t s_printf_lock = JLOS_SPINLOCK_INIT;

void jlos_printk_init(void)
{
    jlos_spinlock_init(&s_printf_lock);
    jlos_hal_serial_default_init();
}

static void printf_scroll_screen(void)
{
    uint16_t *video_memory = JLOS_VGA_TEXT_BUFFER_VA;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            video_memory[80 * y + x] = video_memory[80 * (y + 1) + x];
        }
    }
    for (int x = 0; x < 80; x++) {
        video_memory[80 * 24 + x] = (video_memory[80 * 24 + x] & 0xFF00) | ' ';
    }
}

void printf(const char *str)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    jlos_hal_serial_default_puts(str);
    uint16_t *video_memory = JLOS_VGA_TEXT_BUFFER_VA;
    static uint8_t x = 0, y = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        switch (str[i]) {
            case '\n': y++; x = 0; break;
            default:
                video_memory[80 * y + x] = (video_memory[80 * y + x] & 0xFF00) | str[i];
                x++;
        }
        if (x >= 80) {
            y++;
            x = 0;
        }
        if (y >= 25) {
            printf_scroll_screen();
            y = 24;
            x = 0;
        }
    }
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_hex(uint8_t key)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    char foo[3] = "00";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0F];
    foo[1] = hex[key & 0x0F];
    printf(foo);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_hex16(uint16_t value)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    char foo[5] = "0000";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(value >> 12) & 0x0F];
    foo[1] = hex[(value >> 8) & 0x0F];
    foo[2] = hex[(value >> 4) & 0x0F];
    foo[3] = hex[value & 0x0F];
    printf(foo);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_hex32(uint32_t value)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
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
    printf(foo);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_char(char c)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    char buf[2] = {c, '\0'};
    printf(buf);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

static void printk_putchar(char c)
{
    uint16_t *video_memory = JLOS_VGA_TEXT_BUFFER_VA;
    static uint8_t x = 0, y = 0;
    
    switch (c) {
        case '\n': y++; x = 0; break;
        default:
            video_memory[80 * y + x] = (video_memory[80 * y + x] & 0xFF00) | c;
            x++;
    }
    if (x >= 80) {
        y++;
        x = 0;
    }
    if (y >= 25) {
        printf_scroll_screen();
        y = 24;
        x = 0;
    }
    jlos_hal_serial_default_putc(c);
}

static void printk_puts(const char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        printk_putchar(str[i]);
    }
}

static void printk_itoa(int value, int base)
{
    char buffer[32];
    char *digits = "0123456789ABCDEF";
    int i = 0;
    bool negative = false;
    if (value == 0) {
        printk_putchar('0');
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
        printk_putchar('-');
    }
    while (i > 0) {
        printk_putchar(buffer[--i]);
    }
}

static void printk_utoa(unsigned int value, int base, bool uppercase)
{
    char buffer[32];
    char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    
    if (value == 0) {
        printk_putchar('0');
        return;
    }
    
    while (value > 0) {
        buffer[i++] = digits[value % base];
        value /= base;
    }
    
    while (i > 0) {
        printk_putchar(buffer[--i]);
    }
}

void printk(const char *fmt, ...)
{
    uint32_t *args = (uint32_t *)&fmt + 1;
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    for (int i = 0; fmt[i] != '\0'; i++) {
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
                    printk_utoa(value , 16, false);
                    break;
                }
                case 'X': {
                    unsigned int value = *args++;
                    printk_utoa(value, 16, true);
                    break;
                }
                case 's': {
                    const char *str = (const char *)*args++;
                    printk_puts(str);
                    break;
                }
                case 'c': {
                    char c = (char)*args++;
                    printk_putchar(c);
                    break;
                }
                case 'p': {
                    unsigned int value = *args++;
                    printk_puts("0x");
                    printk_utoa(value, 16, false);
                    break;
                }
                case '%': {
                    printk_putchar('%');
                    break;
                }
                default: {
                    printk_putchar('%');
                    printk_putchar(fmt[i]);
                    break;
                }
            }
        } else {
            printk_putchar(fmt[i]);
        }
    }
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void sysprintf(char *str)
{
    __asm__ __volatile__("int $0x80" : : "a" (4), "b" (str));
}
