#ifndef __JLOS__DRIVERS_ATA_H
#define __JLOS__DRIVERS_ATA_H

#include <common/types.h>
#include <hal/io.h>

typedef struct jlos_ata jlos_ata_t;

struct jlos_ata {
    jlos_io16_t m_data_port;
    jlos_io8_t m_error_port;
    jlos_io8_t m_sector_count_port;
    jlos_io8_t m_lba_low_port;
    jlos_io8_t m_lba_mid_port;
    jlos_io8_t m_lba_hi_port;
    jlos_io8_t m_device_port;
    jlos_io8_t m_command_port;
    jlos_io8_t m_control_port;

    bool m_master;
    uint16_t m_bytes_per_sector;
};

void jlos_ata_init(jlos_ata_t* self, uint16_t m_port_base, bool m_master);
void jlos_ata_destroy(jlos_ata_t* self);

void jlos_ata_identify(jlos_ata_t* self);
void jlos_ata_read28(jlos_ata_t* self, uint32_t sector, uint8_t *m_data, int m_size);
void jlos_ata_write28(jlos_ata_t* self, uint32_t sector, uint8_t *m_data, int m_size);
void jlos_ata_flush(jlos_ata_t* self);

#endif