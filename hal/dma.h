#ifndef __JLOS_HAL_DMA_H
#define __JLOS_HAL_DMA_H

#include <tools/config.h>
#include <common/types.h>

/* 8237 DMA 控制器 HAL：x86 PC 标准配置
 *  - Master (DMA-1): channels 0-3, 8-bit 传输，单次最多 64KB
 *  - Slave  (DMA-2): channels 4-7, 16-bit 传输；channel 4 固定 cascade 给 DMA-1
 *
 * 8237 限制：物理地址必须 < 16MB（24-bit）；长度最大 64KB（DMA-1）/ 128KB（DMA-2 word）。
 * HAL 层做参数检查，超限返回 -EINVAL。 */

#define JLOS_HAL_DMA_CHANNELS   8
#define JLOS_HAL_DMA_CHAN_CASCADE  4   /* 保留，不可用 */

typedef enum {
    JLOS_HAL_DMA_DIR_READ_FROM_DEV = 0,  /* 设备 → 内存（IDE/FDC read 等） */
    JLOS_HAL_DMA_DIR_WRITE_TO_DEV  = 1,  /* 内存 → 设备 */
} jlos_hal_dma_dir_t;

typedef enum {
    JLOS_HAL_DMA_STATUS_IDLE       = 0x00,
    JLOS_HAL_DMA_STATUS_PREPARED   = 0x01,  /* 已写 base/count，还在 masked（未启动） */
    JLOS_HAL_DMA_STATUS_RUNNING    = 0x02,  /* mask 已清，DMA 进行中 */
    JLOS_HAL_DMA_STATUS_TC         = 0x04,  /* 达到 Terminal Count，传输完成 */
    JLOS_HAL_DMA_STATUS_ERROR      = 0x80,
} jlos_hal_dma_status_t;

/* 为每个通道缓存参数（8237 读回来不方便，我们镜像一份） */
typedef struct {
    uint32_t phys_buf;   /* 物理地址，低 24-bit 有效 */
    uint32_t length;     /* 实际传输字节数 */
    jlos_hal_dma_dir_t dir;
    jlos_hal_dma_status_t status;
    uint8_t  used;
} jlos_hal_dma_chan_state_t;

/* 硬件限制：DMA1 ch0-3: 1-byte unit, max 0x10000 bytes (64KB)
 * DMA2 ch5-7: 2-byte unit, max 0x20000 bytes (128KB, in words = 0x10000) */
#define JLOS_HAL_DMA_CH03_MAX_BYTES  0x10000
#define JLOS_HAL_DMA_CH57_MAX_BYTES  0x20000
#define JLOS_HAL_DMA_PHYS_MAX        0x00FFFFFFu  /* 24-bit 地址限制 */

/* ---------------- API ---------------- */

/* 初始化 DMA 控制器（先 all-channel masked，清所有 pending） */
void jlos_hal_dma_init(void);

/* 配置一次 DMA 传输。成功返回 0，失败 < 0：
 *   -1 = 参数错误（bad channel/buf/size）
 *   -2 = 地址 >16MB 超 8237 寻址能力
 *   -3 = 长度超过单通道上限（64KB/128KB）
 *   -4 = 通道 4 cascade 保留，不可用 */
int jlos_hal_dma_prepare(uint8_t channel,
                         uint32_t phys_buf_addr,
                         uint32_t length_bytes,
                         jlos_hal_dma_dir_t dir);

/* 启动：清该 channel mask，DMA 开始传输 */
int jlos_hal_dma_start(uint8_t channel);

/* 停止：置 mask；返回 0 成功 */
int jlos_hal_dma_stop(uint8_t channel);

/* 查询状态，同时返回剩余字节数。
 * out_remaining 可为 NULL。返回 jlos_hal_dma_status_t 的组合 bitmask。 */
jlos_hal_dma_status_t jlos_hal_dma_status(uint8_t channel, uint32_t *out_remaining);

/* 读镜像状态数组（调试用） */
const jlos_hal_dma_chan_state_t *jlos_hal_dma_get_state_table(int *out_count);

#endif
