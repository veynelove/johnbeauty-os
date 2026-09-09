#include <hal/serial.h>
#include <hal/io.h>
#include <hal/hal.h>
#include <hal/diag.h>

#define REG_RBR           0
#define REG_THR           0
#define REG_IER           1
#define REG_IIR           2
#define REG_FCR           2
#define REG_LCR           3
#define REG_MCR           4
#define REG_LSR           5
#define REG_MSR           6
#define REG_SCRATCH       7

#define LCR_DLAB_BIT      0x80
#define UART_INPUT_DIV16  115200U

static uint16_t s_default_com;
static uint8_t  s_default_inited;

static int uart_wait_thr_empty(uint16_t base)
{
    jlos_io8_t lsr;
    jlos_io8_init(&lsr, (uint16_t)(base + REG_LSR));
    int timeout = 100000;
    while (timeout-- > 0) {
        if ((jlos_io8_read(&lsr) & 0x20) != 0) return 1;
    }
    return 0;
}

void jlos_hal_serial_putc(uint16_t com_base, char c)
{
    if (com_base == 0) return;
    (void)uart_wait_thr_empty(com_base);
    jlos_io8_t thr;
    jlos_io8_init(&thr, (uint16_t)(com_base + REG_THR));
    jlos_io8_write(&thr, (uint8_t)c);
}

void jlos_hal_serial_puts(uint16_t com_base, const char *s)
{
    if (!s || com_base == 0) return;
    for (; *s; s++) jlos_hal_serial_putc(com_base, *s);
}

void jlos_hal_serial_default_init(void)
{
    s_default_inited = 0;
    s_default_com = JLOS_HAL_SERIAL_DEFAULT_COM;

    jlos_io8_t ier, lcr, fcr, mcr;
    jlos_io8_init(&ier, (uint16_t)(JLOS_HAL_SERIAL_DEFAULT_COM + REG_IER));
    jlos_io8_init(&lcr, (uint16_t)(JLOS_HAL_SERIAL_DEFAULT_COM + REG_LCR));
    jlos_io8_init(&fcr, (uint16_t)(JLOS_HAL_SERIAL_DEFAULT_COM + REG_FCR));
    jlos_io8_init(&mcr, (uint16_t)(JLOS_HAL_SERIAL_DEFAULT_COM + REG_MCR));

    jlos_io8_write(&ier, 0x00);
    jlos_io8_write(&lcr, LCR_DLAB_BIT);
    uint32_t divisor = UART_INPUT_DIV16 / (uint32_t)JLOS_HAL_SERIAL_DEFAULT_BAUD;
    jlos_io8_t dll, dlm;
    jlos_io8_init(&dll, (uint16_t)(JLOS_HAL_SERIAL_DEFAULT_COM + 0));
    jlos_io8_init(&dlm, (uint16_t)(JLOS_HAL_SERIAL_DEFAULT_COM + 1));
    jlos_io8_write(&dll, (uint8_t)(divisor & 0xFF));
    jlos_io8_write(&dlm, (uint8_t)((divisor >> 8) & 0xFF));
    jlos_io8_write(&lcr, 0x03);
    jlos_io8_write(&fcr, 0xC7);
    jlos_io8_write(&mcr, 0x0B);

    s_default_inited = 1;
}

void jlos_hal_serial_default_putc(char c)
{
    if (!s_default_inited) return;
    jlos_hal_serial_putc(s_default_com, c);
}

void jlos_hal_serial_default_puts(const char *s)
{
    if (!s || !s_default_inited) return;
    jlos_hal_serial_puts(s_default_com, s);
}
