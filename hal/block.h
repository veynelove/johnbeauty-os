#ifndef _JLOS_HAL_BLOCK_H
#define _JLOS_HAL_BLOCK_H

#include <common/types.h>

#define JLOS_HAL_BLOCK_PRIV_SIZE 64

#define JLOS_ATA28_LBA_MAX 0x0FFFFFFFu

typedef enum {
    JLOS_HAL_BLOCK_DEV_ATA_PIO28 = 1,
    JLOS_HAL_BLOCK_DEV_ATA_DMA   = 2,
    JLOS_HAL_BLOCK_DEV_AHCI      = 3,
    JLOS_HAL_BLOCK_DEV_NVME      = 4,
    JLOS_HAL_BLOCK_DEV_PARTITION = 5,
} jlos_hal_block_dev_type_t;

typedef struct jlos_hal_block_dev jlos_hal_block_dev_t;

typedef struct {
    void (*init)(jlos_hal_block_dev_t *self);
    void (*destroy)(jlos_hal_block_dev_t *self);
    void (*identify)(jlos_hal_block_dev_t *self);
    void (*flush)(jlos_hal_block_dev_t *self);
    int  (*read_sectors)(jlos_hal_block_dev_t *self, uint64_t start_lba, uint8_t *buf, uint32_t sector_count);
    int  (*write_sectors)(jlos_hal_block_dev_t *self, uint64_t start_lba, const uint8_t *buf, uint32_t sector_count);
} jlos_hal_block_ops_t;

typedef struct jlos_hal_block_dev {
    jlos_hal_block_dev_type_t   dev_type;
    uint32_t                    bytes_per_sector;
    uint64_t                    total_sectors;
    const jlos_hal_block_ops_t  *ops;
    uint8_t                     priv[JLOS_HAL_BLOCK_PRIV_SIZE];
    uint8_t                     inited;
} jlos_hal_block_dev_t;

void jlos_hal_block_ata_pio28_create(jlos_hal_block_dev_t *self, uint16_t port_base, bool master);

void jlos_hal_block_init(jlos_hal_block_dev_t *self);
void jlos_hal_block_destroy(jlos_hal_block_dev_t *self);
void jlos_hal_block_identify(jlos_hal_block_dev_t *self);
void jlos_hal_block_flush(jlos_hal_block_dev_t *self);

int jlos_hal_block_read(jlos_hal_block_dev_t *self, uint64_t start_lba, uint8_t *buf, uint32_t sector_count);
int jlos_hal_block_write(jlos_hal_block_dev_t *self, uint64_t start_lba, const uint8_t *buf, uint32_t sector_count);

#endif
