#include <hal/dma.h>
#include <hal/io.h>
#include <hal/hal.h>
#include <hal/diag.h>

/* ------ 8237 固定端口 ------
 * DMA-1 (channels 0-3, 8-bit):
 *   base+count regs:  ch0 = 0x00/0x01, ch1=0x02/0x03, ch2=0x04/0x05, ch3=0x06/0x07
 *   command/status: 0x08 (W/R)
 *   request: 0x09
 *   single mask: 0x0A
 *   mode: 0x0B
 *   flip-flop clear: 0x0C (write any)
 *   master clear: 0x0D (write any)
 *   mask clear: 0x0E
 *   multi mask: 0x0F
 *   Page regs: ch0=0x87, ch1=0x83, ch2=0x81, ch3=0x82  (高 8-bit 物理地址)
 * DMA-2 (channels 4-7, 16-bit): ports = 0xC0 + ch*2 ; Page regs: ch5=0x8B, ch6=0x89, ch7=0x8A
 *   DMA-2 regs shifted left by 1 (because CPU uses BYTE ports but DMA works in 16-bit units).
 *   command/status: 0xD0, mask=0xD4, mode=0xD6, FF clear=0xD8, master clear=0xDA, mask clear=0xDC, multi mask=0xDE
 */

/* 简化：实现时统一写 mask/mode/page/base+count，用 HAL IO slow（因为 8237 本身是慢速 ISA 设备）。 */

static jlos_hal_dma_chan_state_t s_state[JLOS_HAL_DMA_CHANNELS];

const jlos_hal_dma_chan_state_t *jlos_hal_dma_get_state_table(int *out_count)
{
    if (out_count) *out_count = JLOS_HAL_DMA_CHANNELS;
    return s_state;
}

/* channel 0-3 单 mask 口 0x0A, channel 4-7 口 0xD4 */
static void dma_write_mask(uint8_t channel, int mask_bit /* 1=mask, 0=unmask */)
{
    uint8_t val;
    if (channel <= 3) {
        jlos_io8_slow_t port;
        jlos_io8_slow_init(&port, 0x0A);
        val = (uint8_t)(((mask_bit & 1) << 2) | (channel & 3));
        jlos_io8_slow_write(&port, val);
    } else {
        jlos_io8_slow_t port;
        jlos_io8_slow_init(&port, 0xD4);
        val = (uint8_t)(((mask_bit & 1) << 2) | ((channel - 4) & 3));
        jlos_io8_slow_write(&port, val);
    }
}

/* 清 byte pointer flip-flop（先写低字节后写高字节） */
static void dma_clear_ff(uint8_t is_dma2)
{
    jlos_io8_slow_t port;
    jlos_io8_slow_init(&port, is_dma2 ? 0xD8 : 0x0C);
    jlos_io8_slow_write(&port, 0x00);
}

/* 写 mode 寄存器 */
static void dma_write_mode(uint8_t channel, jlos_hal_dma_dir_t dir, int is_write /* mode single transfer */)
{
    uint8_t mode;
    /* mode: bits[1:0] channel, [3:2] transfer type (01=Write/10=Read/00=verify), [4]=autoinit off, [5]=addr_inc, [7:6]=00(Demand)/01(Single) */
    if (channel <= 3) {
        mode = (uint8_t)(channel & 3);
    } else {
        mode = (uint8_t)((channel - 4) & 3);
    }
    uint8_t xfer_type = (dir == JLOS_HAL_DMA_DIR_WRITE_TO_DEV) ? 0x08 : 0x04;
    mode = (uint8_t)(mode | xfer_type | 0x40 | 0x00 /* demand default, single is better: 0x40=single 0x41? */);
    /* Use single transfer mode for safety: bits[7:6]=01 */
    mode |= 0x40;
    (void)is_write;
    jlos_io8_slow_t port;
    jlos_io8_slow_init(&port, (channel <= 3) ? 0x0B : 0xD6);
    jlos_io8_slow_write(&port, mode);
}

/* 页寄存器：返回对应 ch 的 page reg 端口；不在范围内返回 0x00 */
static uint16_t dma_page_port(uint8_t ch)
{
    switch (ch) {
        case 0: return 0x87;
        case 1: return 0x83;
        case 2: return 0x81;
        case 3: return 0x82;
        case 5: return 0x8B;
        case 6: return 0x89;
        case 7: return 0x8A;
        default: return 0;
    }
}

/* 基址+计数寄存器对的起始端口（byte 对齐）。DMA-2 ports = 0xC0 + (ch-4)*2. */
static uint16_t dma_basecount_start(uint8_t ch)
{
    if (ch <= 3) return (uint16_t)(ch * 2);     /* 0x00, 0x02, 0x04, 0x06 */
    return (uint16_t)(0xC0 + ((ch - 4) * 2));    /* 0xC0, 0xC2, 0xC4, 0xC6 */
}

void jlos_hal_dma_init(void)
{
    HAL_TRACE_MSG("8237 DMA controller init");
    jlos_hal_register_io_range(0x00, 0x0F, "8237 DMA-1 (ch0-3)");
    jlos_hal_register_io_range(0x80, 0x8F, "DMA Page Registers (74LS612)");
    jlos_hal_register_io_range(0xC0, 0xDF, "8237 DMA-2 (ch4-7 cascade)");
    /* 所有 channel 先 mask（除 cascade 不用管） */
    for (int i = 0; i < JLOS_HAL_DMA_CHANNELS; i++) {
        s_state[i].used = 0;
        s_state[i].status = JLOS_HAL_DMA_STATUS_IDLE;
        if (i != JLOS_HAL_DMA_CHAN_CASCADE) {
            dma_write_mask((uint8_t)i, 1);
        }
    }
    /* master clear both controllers */
    jlos_io8_slow_t clr1;
    jlos_io8_slow_init(&clr1, 0x0D);
    jlos_io8_slow_write(&clr1, 0);
    jlos_io8_slow_t clr2;
    jlos_io8_slow_init(&clr2, 0xDA);
    jlos_io8_slow_write(&clr2, 0);
}

int jlos_hal_dma_prepare(uint8_t channel,
                         uint32_t phys_buf_addr,
                         uint32_t length_bytes,
                         jlos_hal_dma_dir_t dir)
{
    HAL_TRACE_MSG("8237 DMA prepare channel");
    if (channel >= JLOS_HAL_DMA_CHANNELS) return -1;
    if (channel == JLOS_HAL_DMA_CHAN_CASCADE) return -4;
    if (phys_buf_addr > JLOS_HAL_DMA_PHYS_MAX) return -2;

    uint32_t max_bytes;
    if (channel <= 3) max_bytes = JLOS_HAL_DMA_CH03_MAX_BYTES;
    else              max_bytes = JLOS_HAL_DMA_CH57_MAX_BYTES;
    if (length_bytes == 0 || length_bytes > max_bytes) return -3;

    int is_dma2 = (channel >= 4);

    /* 1) mask the channel before programming */
    dma_write_mask(channel, 1);

    /* 2) clear flip-flop */
    dma_clear_ff((uint8_t)is_dma2);

    /* 3) write mode */
    dma_write_mode(channel, dir, 1);

    /* 4) write base addr (low/high byte, 16 bits). DMA-2: addr is 16-bit word addr,
     *    but CPU still accesses via byte ports. 16-bit transfers use A1-A16, so
     *    A0 is ignored. phys_buf_addr must be EVEN for DMA2. */
    uint16_t bc_start = dma_basecount_start(channel);
    jlos_io8_slow_t base_lo, base_hi;
    jlos_io8_slow_init(&base_lo, bc_start);
    jlos_io8_slow_init(&base_hi, (uint16_t)(bc_start + 1));

    uint32_t effective_phys = phys_buf_addr;
    if (is_dma2) {
        if ((effective_phys & 1u) != 0) return -1;  /* DMA-2 16-bit must be 2-byte aligned */
    }
    uint16_t addr16;
    if (is_dma2) {
        addr16 = (uint16_t)((effective_phys >> 1) & 0xFFFFu);  /* shift to word address */
    } else {
        addr16 = (uint16_t)(effective_phys & 0xFFFFu);
    }
    jlos_io8_slow_write(&base_lo, (uint8_t)(addr16 & 0xFF));
    jlos_io8_slow_write(&base_hi, (uint8_t)((addr16 >> 8) & 0xFF));

    /* 5) write page register (high 8 bits of 24-bit physical byte addr, always) */
    uint16_t pgport = dma_page_port(channel);
    if (pgport != 0) {
        jlos_io8_slow_t pg;
        jlos_io8_slow_init(&pg, pgport);
        uint8_t page_byte = (uint8_t)((effective_phys >> 16) & 0xFFu);
        jlos_io8_slow_write(&pg, page_byte);
    }

    /* 6) write count: (length / unit) - 1. */
    uint16_t count_reg;
    if (is_dma2) {
        count_reg = (uint16_t)((length_bytes / 2u) - 1u);
    } else {
        count_reg = (uint16_t)(length_bytes - 1u);
    }
    jlos_io8_slow_t cnt_lo, cnt_hi;
    jlos_io8_slow_init(&cnt_lo, (uint16_t)(bc_start + 0));
    jlos_io8_slow_init(&cnt_hi, (uint16_t)(bc_start + 1));
    (void)cnt_lo; (void)cnt_hi;
    /* Registers: base+0 is addr, base+1 is count. 对 DMA1: 0x00=addr, 0x01=count */
    jlos_io8_slow_t count_port_lo, count_port_hi;
    jlos_io8_slow_init(&count_port_lo, (uint16_t)(bc_start + 1));
    jlos_io8_slow_init(&count_port_hi, (uint16_t)(bc_start + 1));
    dma_clear_ff((uint8_t)is_dma2);
    jlos_io8_slow_write(&count_port_lo, (uint8_t)(count_reg & 0xFF));
    jlos_io8_slow_write(&count_port_hi, (uint8_t)((count_reg >> 8) & 0xFF));

    /* 镜像保存 */
    s_state[channel].phys_buf = effective_phys;
    s_state[channel].length = length_bytes;
    s_state[channel].dir = dir;
    s_state[channel].status = JLOS_HAL_DMA_STATUS_PREPARED;
    s_state[channel].used = 1;
    return 0;
}

int jlos_hal_dma_start(uint8_t channel)
{
    HAL_TRACE_MSG("8237 DMA start channel");
    if (channel >= JLOS_HAL_DMA_CHANNELS) return -1;
    if (channel == JLOS_HAL_DMA_CHAN_CASCADE) return -4;
    if (!s_state[channel].used ||
        !(s_state[channel].status & JLOS_HAL_DMA_STATUS_PREPARED)) return -1;
    dma_write_mask(channel, 0);
    s_state[channel].status = JLOS_HAL_DMA_STATUS_RUNNING;
    return 0;
}

int jlos_hal_dma_stop(uint8_t channel)
{
    if (channel >= JLOS_HAL_DMA_CHANNELS) return -1;
    if (channel == JLOS_HAL_DMA_CHAN_CASCADE) return -4;
    dma_write_mask(channel, 1);
    if (s_state[channel].status == JLOS_HAL_DMA_STATUS_RUNNING) {
        s_state[channel].status = JLOS_HAL_DMA_STATUS_PREPARED;
    }
    return 0;
}

jlos_hal_dma_status_t jlos_hal_dma_status(uint8_t channel, uint32_t *out_remaining)
{
    if (channel >= JLOS_HAL_DMA_CHANNELS) return JLOS_HAL_DMA_STATUS_ERROR;
    if (out_remaining) {
        if (!s_state[channel].used) {
            *out_remaining = 0;
        } else {
            /* 简化：返回镜像 length。未来可通过读当前 count reg 计算 remaining。 */
            *out_remaining = s_state[channel].length;
        }
    }
    return s_state[channel].status;
}
