#ifndef _JLOS__DRIVERS_ATA_H
#define _JLOS__DRIVERS_ATA_H

#include <common/types.h>
#include <hal/io.h>
#include <hal/block.h>

extern const uint16_t jlos_ata_primary_port_base;
extern const uint16_t jlos_ata_secondary_port_base;

typedef struct jlos_ata {
    jlos_io16_t data_port;
    jlos_io8_t  error_port;
    jlos_io8_t  sector_count_port;
    jlos_io8_t  lba_low_port;
    jlos_io8_t  lba_mid_port;
    jlos_io8_t  lba_hi_port;
    jlos_io8_t  device_port;
    jlos_io8_t  command_port;
    jlos_io8_t  control_port;
    bool        master;
    uint16_t    bytes_per_sector;
    uint64_t    total_sectors;
} jlos_ata_t;

void jlos_ata_init(jlos_ata_t* self, uint16_t port_base, bool master);
void jlos_ata_destroy(jlos_ata_t* self);

void jlos_ata_identify(jlos_ata_t* self);
void jlos_ata_read28(jlos_ata_t* self, uint32_t sector, uint8_t *data, int size);
void jlos_ata_write28(jlos_ata_t* self, uint32_t sector, uint8_t *data, int size);
void jlos_ata_flush(jlos_ata_t* self);
jlos_hal_block_dev_t *jlos_ata_get_primary_dev(void);

#endif
