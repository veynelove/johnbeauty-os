#include <kernel/printk.h>
#include <hal/spinlock.h>
#include <hal/serial.h>

static jlos_spinlock_t s_printf_lock = JLOS_SPINLOCK_INIT;

void jlos_printk_init(void)
{
    jlos_spinlock_init(&s_printf_lock);
    jlos_hal_serial_default_init();
}

static void printf_scroll_screen(void)
{
    static uint16_t *video_memory = (uint16_t *)0xb8000;
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
    static uint16_t *video_memory = (uint16_t *)0xb8000;
    static uint8_t m_x = 0, m_y = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        switch (str[i]) {
            case '\n': m_y++; m_x = 0; break;
            default:
                video_memory[80 * m_y + m_x] = (video_memory[80 * m_y + m_x] & 0xFF00) | str[i];
                m_x++;
        }
        if (m_x >= 80) {
            m_y++;
            m_x = 0;
        }
        if (m_y >= 25) {
            printf_scroll_screen();
            m_y = 24;
            m_x = 0;
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

void sysprintf(char *str)
{
    __asm__ __volatile__("int $0x80" : : "a" (4), "b" (str));
}
