#ifndef __JLOS_FILESYSTEM_MSDOSPATH_H
#define __JLOS_FILESYSTEM_MSDOSPATH_H

#include <drivers/ata.h>

typedef struct {
    uint8_t bootable;
    uint8_t start_head;
    uint8_t start_sector;
    uint16_t start_cylinder;
    uint8_t partition_id;
    uint8_t end_head;
    uint8_t end_sector;
    uint16_t end_cylinder;
    uint32_t start_lba;
    uint32_t length;
} __attribute__((packed)) jlos_partition_table_entry_t;

typedef struct {
    uint8_t bootloader[440];
    uint32_t signature;
    uint16_t unused;
    jlos_partition_table_entry_t primary_partition[4];
    uint16_t magicnumber;
} __attribute__((packed)) jlos_master_boot_record_t;

void jlos_msdos_partition_table_read_partitions(jlos_ata_t *hd);

#endif