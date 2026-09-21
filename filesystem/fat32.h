#ifndef _JLOS_FILESYSTEM_FAT32_H
#define _JLOS_FILESYSTEM_FAT32_H

#include <filesystem/vfs.h>
#include <hal/block.h>

#define JLOS_FAT32_ATTR_READ_ONLY   0x01
#define JLOS_FAT32_ATTR_HIDDEN      0x02
#define JLOS_FAT32_ATTR_SYSTEM      0x04
#define JLOS_FAT32_ATTR_VOLUME_ID   0x08
#define JLOS_FAT32_ATTR_DIRECTORY   0x10
#define JLOS_FAT32_ATTR_ARCHIVE     0x20
#define JLOS_FAT32_ATTR_LFN         0x0f

#define JLOS_FAT32_CLUSTER_FREE     0x00000000
#define JLOS_FAT32_CLUSTER_BAD      0x0FFFFFF7
#define JLOS_FAT32_CLUSTER_EOC      0x0FFFFFF8
#define JLOS_FAT32_CLUSTER_MASK     0x0FFFFFFF

#define JLOS_FAT32_BYTES_PER_SECTOR     512
#define JLOS_FAT32_FIRST_DATA_CLUSTER   2
#define JLOS_FAT32_SHORT_NAME_LEN       8
#define JLOS_FAT32_SHORT_EXT_LEN        3

#define JLOS_FAT32_DIRENT_END           0x00
#define JLOS_FAT32_DIRENT_DELETED       0xE5

#define JLOS_FAT32_JUMP_LEN             3
#define JLOS_FAT32_OEM_NAME_LEN         8
#define JLOS_FAT32_VOLUME_LABEL_LEN     11
#define JLOS_FAT32_TYPE_LABEL_LEN       8
#define JLOS_FAT32_BPB_RESERVED_LEN     12

typedef struct {
    uint8_t     jump[JLOS_FAT32_JUMP_LEN];
    uint8_t     soft_name[JLOS_FAT32_OEM_NAME_LEN];
    uint16_t    bytes_per_sector;
    uint8_t     sector_per_cluster;
    uint16_t    reserved_sectors;
    uint8_t     fat_copies;
    uint16_t    root_dir_entries;
    uint16_t    total_sectors;
    uint8_t     media_type;
    uint16_t    fat_sector_count;
    uint16_t    sectors_per_track;
    uint16_t    head_count;
    uint32_t    hidden_sectors;
    uint32_t    total_sector_count;
    
    uint32_t    table_size;
    uint16_t    ext_flags;
    uint16_t    fat_version;
    uint32_t    root_cluster;
    uint16_t    fat_info;
    uint16_t    backup_sector;
    uint8_t     reserved0[JLOS_FAT32_BPB_RESERVED_LEN];
    uint8_t     drive_number;
    uint8_t     reserved;
    uint8_t     boot_signature;
    uint32_t    volume_id;
    uint8_t     volume_label[JLOS_FAT32_VOLUME_LABEL_LEN];
    uint8_t     fat_type_label[JLOS_FAT32_TYPE_LABEL_LEN];
} __attribute__((packed)) jlos_fat32_bpb_t; //BIOS Parameter Block

typedef struct {
    uint8_t     name[JLOS_FAT32_SHORT_NAME_LEN];
    uint8_t     ext[JLOS_FAT32_SHORT_EXT_LEN];
    uint8_t     attributes;
    uint8_t     reserved;
    uint8_t     c_time_tenth;
    uint16_t    c_time;
    uint16_t    c_date;
    uint16_t    a_time;
    uint16_t    first_cluster_hi;
    uint16_t    w_time;
    uint16_t    w_date;
    uint16_t    first_cluster_low;
    uint32_t    size;
} __attribute__((packed)) jlos_fat32_dirent_t;

typedef struct {
    jlos_fat32_bpb_t    bpb;
    uint32_t            fat_start;
    uint32_t            data_start;
    uint32_t            root_cluster;
    uint32_t            bytes_per_cluster;
    uint32_t            sectors_per_cluster;
    uint32_t            fat_copies;
    uint32_t            total_clusters;
} jlos_fat32_sb_info_t;

typedef struct {
    uint32_t first_cluster;
} jlos_fat32_inode_info_t;

extern jlos_vfs_fs_type_t g_fat32_fs_type;

#endif
