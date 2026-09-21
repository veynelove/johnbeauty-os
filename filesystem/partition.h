#ifndef _JLOS_FILESYSTEM_PARTITION_H
#define _JLOS_FILESYSTEM_PARTITION_H

#include <hal/block.h>

#define JLOS_MBR_MAGIC                          0xAA55
#define JLOS_MBR_MAX_PARTITIONS                 4
#define JLOS_MBR_BOOTABLE_FLAG                  0x80

#define JLOS_PARTITION_TYPE_FAT32_LBA           0x0B
#define JLOS_PARTITION_TYPE_FAT32_LBA_INT113    0x0C
#define JLOS_PARTITION_TYPE_NTFS                0x07

typedef struct {
    uint8_t     bootable;
    uint8_t     start_head;
    uint8_t     start_sector;
    uint8_t     start_cylinder;
    uint8_t     partition_id;
    uint8_t     end_head;
    uint8_t     end_sector;
    uint8_t     end_cylinder;
    uint32_t    start_lba;
    uint32_t    length;
} __attribute__((packed)) jlos_partition_table_entry_t;

typedef struct {
    uint8_t bootloader[440];
    uint32_t signature;
    uint16_t unused;
    jlos_partition_table_entry_t primary_partition[JLOS_MBR_MAX_PARTITIONS];
    uint16_t magicnumber;
} __attribute__((packed)) jlos_master_boot_record_t;

typedef struct {
    jlos_hal_block_dev_t    base;
    jlos_hal_block_dev_t    *physical;
    uint64_t                lba_offset;
    uint64_t                sector_count;
} jlos_partition_dev_t;

void jlos_partition_dev_create(jlos_partition_dev_t *self, jlos_hal_block_dev_t *physical, uint64_t lba_offset, uint64_t sector_count);
int jlos_partition_parse_mbr(jlos_hal_block_dev_t *dev, jlos_partition_table_entry_t *entries, int max_entries);

#endif
