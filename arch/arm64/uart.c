/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/serial.h>
#include <hal/io.h>
#include <hal/paging.h>
#include <common/types.h>

#define JLOS_ARM64_UART0_BASE           (0x09000000UL + KERNEL_DIRECT_MAP_BASE)

#define JLOS_PL011_REG_DR               0x00
#define JLOS_PL011_REG_FR               0x18
#define JLOS_PL011_REG_IBRD             0x24
#define JLOS_PL011_REG_FBRD             0x28
#define JLOS_PL011_REG_LCR_H            0x2C
#define JLOS_PL011_REG_CR               0x30
#define JLOS_PL011_REG_IMSC             0x38
#define JLOS_PL011_REG_ICR              0x44

#define JLOS_PL011_FR_BUSY              (1U << 3)
#define JLOS_PL011_FR_TXFF              (1U << 5)

#define JLOS_PL011_LCR_H_FEN            (1U << 4)
#define JLOS_PL011_LCR_H_WLEN_8         (3U << 5)

#define JLOS_PL011_CR_UARTEN            (1U << 0)
#define JLOS_PL011_CR_TXE               (1U << 1)
#define JLOS_PL011_CR_RXE               (1U << 4)

#define JLOS_PL011_ICR_ALL              0x7FFU

#define JLOS_PL011_UART0_CLK_HZ         24000000UL
#define JLOS_PL011_BAUD_DEFAULT         115200UL
#define JLOS_PL011_BAUD_DIV_MIN         16UL
#define JLOS_PL011_BAUD_FRAC_BITS       64UL
#define JLOS_PL011_BAUD_FRAC_MASK       0x3FUL

static uintptr_t s_uart_base;
static bool      s_uart_inited;

static uint32_t pl011_reg_read(uint32_t offset)
{
    return jlos_io_mmio_read32((const volatile void *)(s_uart_base + offset));
}

static void pl011_reg_write(uint32_t offset, uint32_t val)
{
    jlos_io_mmio_write32((volatile void *)(s_uart_base + offset), val);
}

static void pl011_wait_tx_fifo_not_full(void)
{
    while ((pl011_reg_read(JLOS_PL011_REG_FR) & JLOS_PL011_FR_TXFF) != 0) {
        /* busy-wait until TX FIFO has room */
    }
}

void jlos_hal_serial_default_init(void)
{
    uint64_t divisor_x64;
    uint32_t ibrd, fbrd;

    s_uart_base = JLOS_ARM64_UART0_BASE;

    pl011_reg_write(JLOS_PL011_REG_IMSC, 0);
    pl011_reg_write(JLOS_PL011_REG_ICR, JLOS_PL011_ICR_ALL);
    pl011_reg_write(JLOS_PL011_REG_CR, 0);              /* disable first */

    /* BaudDiv = clk / (16 * baud), in x64 fixed point: IBRD >> 6, FBRD & 0x3F
     * 24MHz / (16 * 115200) = 13.0208 -> IBRD=13, FBRD=1 */
    divisor_x64  = JLOS_PL011_UART0_CLK_HZ * JLOS_PL011_BAUD_FRAC_BITS;
    divisor_x64 /= JLOS_PL011_BAUD_DIV_MIN * JLOS_PL011_BAUD_DEFAULT;
    ibrd = (uint32_t)(divisor_x64 >> 6);
    fbrd = (uint32_t)(divisor_x64 & JLOS_PL011_BAUD_FRAC_MASK);

    pl011_reg_write(JLOS_PL011_REG_IBRD, ibrd);
    pl011_reg_write(JLOS_PL011_REG_FBRD, fbrd);
    pl011_reg_write(JLOS_PL011_REG_LCR_H, JLOS_PL011_LCR_H_FEN | JLOS_PL011_LCR_H_WLEN_8);
    pl011_reg_write(JLOS_PL011_REG_CR, JLOS_PL011_CR_UARTEN | JLOS_PL011_CR_TXE | JLOS_PL011_CR_RXE);

    while ((pl011_reg_read(JLOS_PL011_REG_FR) & JLOS_PL011_FR_BUSY) != 0) {
        /* drain before first write */
    }

    s_uart_inited = true;
}

void jlos_hal_serial_default_putc(char c)
{
    if (!s_uart_inited) return;
    pl011_wait_tx_fifo_not_full();
    pl011_reg_write(JLOS_PL011_REG_DR, (uint32_t)(uint8_t)c);
}

void jlos_hal_serial_default_puts(const char *s)
{
    if (!s || !s_uart_inited) return;
    for (; *s != '\0'; s++) jlos_hal_serial_default_putc(*s);
}
