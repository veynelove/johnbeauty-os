#ifndef _JLOS_HAL_DMA_H
#define _JLOS_HAL_DMA_H

#include <tools/config.h>
#include <common/types.h>

#define JLOS_HAL_DMA_CHANNELS       8
#define JLOS_HAL_DMA_CHAN_CASCADE   4

#define JLOS_HAL_DMA_CH03_MAX_BYTES  0x10000
#define JLOS_HAL_DMA_CH57_MAX_BYTES  0x20000
#define JLOS_HAL_DMA_PHYS_MAX        0x00FFFFFFu  

typedef enum {
    JLOS_HAL_DMA_DIR_READ_FROM_DEV = 0,
    JLOS_HAL_DMA_DIR_WRITE_TO_DEV  = 1,
} jlos_hal_dma_dir_t;

typedef enum {
    JLOS_HAL_DMA_STATUS_IDLE       = 0x00,
    JLOS_HAL_DMA_STATUS_PREPARED   = 0x01,
    JLOS_HAL_DMA_STATUS_RUNNING    = 0x02,
    JLOS_HAL_DMA_STATUS_TC         = 0x04,
    JLOS_HAL_DMA_STATUS_ERROR      = 0x80,
} jlos_hal_dma_status_t;

typedef struct {
    uint32_t                phys_buf;
    uint32_t                length;   
    jlos_hal_dma_dir_t      dir;
    jlos_hal_dma_status_t   status;
    uint8_t                 used;
} jlos_hal_dma_chan_state_t;

void jlos_hal_dma_init(void);

int jlos_hal_dma_prepare(uint8_t channel, uint32_t phys_buf_addr, uint32_t length_bytes, jlos_hal_dma_dir_t dir);
int jlos_hal_dma_start(uint8_t channel);
int jlos_hal_dma_stop(uint8_t channel);

jlos_hal_dma_status_t jlos_hal_dma_status(uint8_t channel, uint32_t *out_remaining);
const jlos_hal_dma_chan_state_t *jlos_hal_dma_get_state_table(int *out_count);

#endif
