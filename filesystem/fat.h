#ifndef _JLOS_FILESYSTEM_FAT_H
#define _JLOS_FILESYSTEM_FAT_H

#include <drivers/ata.h>

typedef struct {
    uint8_t jump[3];
    uint8_t soft_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_copies;
    uint16_t root_dir_entries;
    uint16_t total_sectors;
    uint8_t media_type;
    uint16_t fat_sector_count;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sector_count;

    uint32_t table_size;
    uint16_t ext_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fat_info;
    uint16_t backup_sector;
    uint8_t reserved0[12];
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_labe[11];
    uint8_t fat_type_labe[8];
} __attribute__((packed)) jlos_bios_parameter_block32_t;

typedef struct {
    uint8_t name[8];
    uint8_t ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t c_time_tenth;
    uint16_t c_time;
    uint16_t c_date;
    uint16_t a_time;

    uint16_t first_cluster_hi;
    uint16_t w_time;
    uint16_t w_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) jlos_directory_entry_fat32_t;

void jlos_read_bios_block(jlos_ata_t *hd, uint32_t partition_offset);

#endif