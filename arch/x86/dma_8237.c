#include <hal/dma.h>
#include <hal/io.h>
#include <hal/hal.h>
#include <hal/diag.h>

#define JLOS_DMA_CHANNELS         8
#define JLOS_DMA_CHAN_CASCADE     4
#define JLOS_DMA_CH03_MAX_BYTES   0x10000
#define JLOS_DMA_CH57_MAX_BYTES   0x20000
#define JLOS_DMA_PHYS_MAX         0x00FFFFFFu

static jlos_hal_dma_chan_state_t s_state[JLOS_DMA_CHANNELS];

const jlos_hal_dma_chan_state_t *jlos_hal_dma_get_state_table(int *out_count)
{
    if (out_count) *out_count = JLOS_DMA_CHANNELS;
    return s_state;
}

static void dma_write_mask(uint8_t channel, int mask_bit )
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

static void dma_clear_ff(uint8_t is_dma2)
{
    jlos_io8_slow_t port;
    jlos_io8_slow_init(&port, is_dma2 ? 0xD8 : 0x0C);
    jlos_io8_slow_write(&port, 0x00);
}

static void dma_write_mode(uint8_t channel, jlos_hal_dma_dir_t dir, int is_write )
{
    uint8_t mode;
    if (channel <= 3) {
        mode = (uint8_t)(channel & 3);
    } else {
        mode = (uint8_t)((channel - 4) & 3);
    }
    uint8_t xfer_type = (dir == JLOS_HAL_DMA_DIR_WRITE_TO_DEV) ? 0x08 : 0x04;
    mode = (uint8_t)(mode | xfer_type | 0x40);
    (void)is_write;
    jlos_io8_slow_t port;
    jlos_io8_slow_init(&port, (channel <= 3) ? 0x0B : 0xD6);
    jlos_io8_slow_write(&port, mode);
}

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

static uint16_t dma_basecount_start(uint8_t ch)
{
    if (ch <= 3) return (uint16_t)(ch * 2);     
    return (uint16_t)(0xC0 + ((ch - 4) * 2));    
}

void jlos_hal_dma_init(void)
{
    HAL_TRACE_MSG("8237 DMA controller init");
    jlos_hal_register_io_range(0x00, 0x0F, "8237 DMA-1 (ch0-3)");
    jlos_hal_register_io_range(0x80, 0x8F, "DMA Page Registers (74LS612)");
    jlos_hal_register_io_range(0xC0, 0xDF, "8237 DMA-2 (ch4-7 cascade)");
    
    for (int i = 0; i < JLOS_DMA_CHANNELS; i++) {
        s_state[i].used = 0;
        s_state[i].status = JLOS_HAL_DMA_STATUS_IDLE;
        if (i != JLOS_DMA_CHAN_CASCADE) {
            dma_write_mask((uint8_t)i, 1);
        }
    }
    
    jlos_io8_slow_t clr1;
    jlos_io8_slow_init(&clr1, 0x0D);
    jlos_io8_slow_write(&clr1, 0);
    jlos_io8_slow_t clr2;
    jlos_io8_slow_init(&clr2, 0xDA);
    jlos_io8_slow_write(&clr2, 0);
}

int jlos_hal_dma_prepare(uint8_t channel, uint32_t phys_buf_addr, uint32_t length_bytes, jlos_hal_dma_dir_t dir)
{
    HAL_TRACE_MSG("8237 DMA prepare channel");
    if (channel >= JLOS_DMA_CHANNELS) return -1;
    if (channel == JLOS_DMA_CHAN_CASCADE) return -4;
    if (phys_buf_addr > JLOS_DMA_PHYS_MAX) return -2;

    uint32_t max_bytes;
    if (channel <= 3) max_bytes = JLOS_DMA_CH03_MAX_BYTES;
    else              max_bytes = JLOS_DMA_CH57_MAX_BYTES;
    if (length_bytes == 0 || length_bytes > max_bytes) return -3;

    int is_dma2 = (channel >= 4);
    dma_write_mask(channel, 1);
    dma_clear_ff((uint8_t)is_dma2);
    dma_write_mode(channel, dir, 1);

    uint16_t bc_start = dma_basecount_start(channel);

    uint32_t effective_phys = phys_buf_addr;
    if (is_dma2) {
        if ((effective_phys & 1u) != 0) return -1;  
    }
    uint16_t addr16;
    if (is_dma2) {
        addr16 = (uint16_t)((effective_phys >> 1) & 0xFFFFu);  
    } else {
        addr16 = (uint16_t)(effective_phys & 0xFFFFu);
    }
    jlos_io8_slow_t addr_port;
    jlos_io8_slow_init(&addr_port, bc_start);
    jlos_io8_slow_write(&addr_port, (uint8_t)(addr16 & 0xFF));
    jlos_io8_slow_write(&addr_port, (uint8_t)((addr16 >> 8) & 0xFF));

    uint16_t pgport = dma_page_port(channel);
    if (pgport != 0) {
        jlos_io8_slow_t pg;
        jlos_io8_slow_init(&pg, pgport);
        uint8_t page_byte = (uint8_t)((effective_phys >> 16) & 0xFFu);
        jlos_io8_slow_write(&pg, page_byte);
    }

    uint16_t count_reg;
    if (is_dma2) {
        count_reg = (uint16_t)((length_bytes / 2u) - 1u);
    } else {
        count_reg = (uint16_t)(length_bytes - 1u);
    }
    
    jlos_io8_slow_t count_port;
    jlos_io8_slow_init(&count_port, (uint16_t)(bc_start + 1));
    dma_clear_ff((uint8_t)is_dma2);
    jlos_io8_slow_write(&count_port, (uint8_t)(count_reg & 0xFF));
    jlos_io8_slow_write(&count_port, (uint8_t)((count_reg >> 8) & 0xFF));
    
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
    if (channel >= JLOS_DMA_CHANNELS) return -1;
    if (channel == JLOS_DMA_CHAN_CASCADE) return -4;
    if (!s_state[channel].used ||
        !(s_state[channel].status & JLOS_HAL_DMA_STATUS_PREPARED)) return -1;
    dma_write_mask(channel, 0);
    s_state[channel].status = JLOS_HAL_DMA_STATUS_RUNNING;
    return 0;
}

int jlos_hal_dma_stop(uint8_t channel)
{
    if (channel >= JLOS_DMA_CHANNELS) return -1;
    if (channel == JLOS_DMA_CHAN_CASCADE) return -4;
    dma_write_mask(channel, 1);
    if (s_state[channel].status == JLOS_HAL_DMA_STATUS_RUNNING) {
        s_state[channel].status = JLOS_HAL_DMA_STATUS_PREPARED;
    }
    return 0;
}

jlos_hal_dma_status_t jlos_hal_dma_status(uint8_t channel, uint32_t *out_remaining)
{
    if (channel >= JLOS_DMA_CHANNELS) return JLOS_HAL_DMA_STATUS_ERROR;
    if (out_remaining) {
        if (!s_state[channel].used) {
            *out_remaining = 0;
        } else {
            *out_remaining = s_state[channel].length;
        }
    }
    return s_state[channel].status;
}
