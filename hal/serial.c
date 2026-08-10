#include <hal/serial.h>
#include <hal/io.h>
#include <hal/hal.h>
#include <hal/diag.h>

/* 16550 寄存器偏移（DLAB=0） */
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

/* DLAB=1 时 REG0=divisor low, REG1=divisor high */
#define LCR_DLAB_BIT      0x80

/* 期望波特率 = 115200 Hz / divisor（16550 通用时钟 1.8432 MHz ÷ 16 = 115200） */
#define UART_INPUT_DIV16  115200U

/* 默认 COM 缓存，避免 jlos_hal_serial_default_* 每次传参数 */
static uint16_t s_default_com;
static uint8_t  s_default_inited;

/* 内部：等待 THR 为空（LSR bit5=1）。返回 1 = 就绪。 */
static int uart_wait_thr_empty(uint16_t base)
{
    jlos_io8_t lsr;
    jlos_io8_init(&lsr, (uint16_t)(base + REG_LSR));
    int timeout = 100000;  /* 大约几 ms，防止死锁 */
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
    /* hal_arch_init() 已经把 0x3F8-0x3FF 注册成 "16550 UART COM1" 保留段，
     * 这里直接 init 硬件，不会注册重复段。 */
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


