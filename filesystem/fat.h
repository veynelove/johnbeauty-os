#ifndef __JLOS_FILESYSTEM_FAT_H
#define __JLOS_FILESYSTEM_FAT_H

#include <drivers/ata.h>

namespace JLOS {
namespace FileSystem {
struct bios_parameter_block32 {
     uint8_t jump{3};
     uint8_t soft_name{8};
     uint16_t m_bytes_per_sector;
     uint8_t m_sectors_per_cluster;
     uint16_t m_reserved_sectors;
     uint8_t m_fat_copies;
     uint16_t m_root_dir_entries;
     uint16_t m_total_sectors;
     uint8_t m_media_type;
     uint16_t m_fat_sector_count;
     uint16_t m_sectors_per_track;
     uint16_t m_head_count;
     uint32_t m_hidden_sectors;
     uint32_t m_total_sector_count;

     uint32_t m_table_size;
     uint16_t m_ext_flags;
     uint16_t m_fat_version;
     uint32_t m_root_cluster;
     uint16_t m_fat_info;
     uint16_t m_backup_sector;
     uint8_t reserved0[12];
     uint8_t m_drive_number;
     uint8_t m_reserved;
     uint8_t m_boot_signature;
     uint32_t m_volume_id;
     uint8_t volume_labe[11];
     uint8_t fat_type_labe[8];
} __attribute__((packed));

struct directory_entry_fat32 {
     uint8_t name[8];
     uint8_t ext[3];
     uint8_t m_attributes;
     uint8_t m_reserved;
     uint8_t m_c_time_tenth;
     uint16_t m_c_time;
     uint16_t m_c_date;
     uint16_t m_a_time;

     uint16_t m_first_cluster_hi;
     uint16_t m_w_time;
     uint16_t m_w_date;
     uint16_t m_first_cluster_low;
     uint32_t m_size;
} __attribute__((packed));

void read_bios_block(Drivers::advanced_technolog_attachment *hd, uint32_t partition_offset);
}
}
#endif
