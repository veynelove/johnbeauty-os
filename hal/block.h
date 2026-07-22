#ifndef __JLOS_HAL_BLOCK_H
#define __JLOS_HAL_BLOCK_H

#include <tools/config.h>
#include <common/types.h>
#include <drivers/ata.h>

/* 统一块设备 HAL：上层（文件系统 / VFS）只调 HAL 接口，
 * 底层具体是 ATA PIO28 / ATA DMA / AHCI / NVMe / SD 都透明。 */

typedef enum {
    JLOS_HAL_BLOCK_DEV_ATA_PIO28 = 1,  /* 当前已实现 */
    JLOS_HAL_BLOCK_DEV_ATA_DMA,
    JLOS_HAL_BLOCK_DEV_AHCI,
    JLOS_HAL_BLOCK_DEV_NVME,
} jlos_hal_block_dev_type_t;

/* 块设备操作函数表；未来每个后端各填一套 */
typedef struct jlos_hal_block_dev jlos_hal_block_dev_t;

typedef struct {
    void (*init)(jlos_hal_block_dev_t *self);
    void (*destroy)(jlos_hal_block_dev_t *self);
    void (*identify)(jlos_hal_block_dev_t *self);
    void (*flush)(jlos_hal_block_dev_t *self);
    /* sector 大小以 bytes 为单位；count = 连续扇区数。返回 0=成功，负=失败 */
    int  (*read_sectors)(jlos_hal_block_dev_t *self, uint64_t start_lba,
                         uint8_t *buf, uint32_t sector_count);
    int  (*write_sectors)(jlos_hal_block_dev_t *self, uint64_t start_lba,
                          const uint8_t *buf, uint32_t sector_count);
} jlos_hal_block_ops_t;

struct jlos_hal_block_dev {
    jlos_hal_block_dev_type_t dev_type;
    uint32_t bytes_per_sector;
    uint64_t total_sectors;    /* 0 = 未 identify 或未知 */
    const jlos_hal_block_ops_t *ops;
    /* 私有区，后端具体实现自由使用（我们这里放 ATA 对象） */
    union {
        jlos_ata_t ata;
    } dev_priv;
    uint8_t inited;
};

/* ---------------- ATA PIO-28 后端构造函数 ----------------
 * 底层用 <drivers/ata.h> 的 jlos_ata 实现。
 * - port_base: 通常 0x1F0 (Primary) 或 0x170 (Secondary)
 * - master: true = Master 盘, false = Slave 盘 */
void jlos_hal_block_ata_pio28_create(jlos_hal_block_dev_t *self,
                                     uint16_t port_base, bool master);

/* ---------------- 通用块设备 API（上层只调这些） ---------------- */
void jlos_hal_block_init(jlos_hal_block_dev_t *self);
void jlos_hal_block_destroy(jlos_hal_block_dev_t *self);
void jlos_hal_block_identify(jlos_hal_block_dev_t *self);
void jlos_hal_block_flush(jlos_hal_block_dev_t *self);

/* 返回 0 = 成功；-1 = 参数错误 / 未 init / 不支持（比如 LBA 超过 28-bit） */
int jlos_hal_block_read(jlos_hal_block_dev_t *self, uint64_t start_lba,
                        uint8_t *buf, uint32_t sector_count);
int jlos_hal_block_write(jlos_hal_block_dev_t *self, uint64_t start_lba,
                         const uint8_t *buf, uint32_t sector_count);

#endif
