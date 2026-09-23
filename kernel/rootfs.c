#include <filesystem/vfs.h>
#include <filesystem/partition.h>
#include <drivers/ata.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "rootfs"
#include <kernel/printk.h>

static jlos_partition_dev_t s_root_partition;

static void jlos_rootfs_init(void)
{
    jlos_hal_block_dev_t *ata_dev = jlos_ata_get_primary_dev();
    if (!ata_dev) {
        printk_err("no ata device available\n");
        return;
    }
    jlos_partition_table_entry_t entries[JLOS_MBR_MAX_PARTITIONS];
    int num = jlos_partition_parse_mbr(ata_dev, entries, JLOS_MBR_MAX_PARTITIONS);
    if (num <= 0) {
        printk_err("no partitions found\n");
        return;
    }
    for (int i = 0; i < num; i++) {
        if (entries[i].partition_id != JLOS_PARTITION_TYPE_FAT32_LBA 
        && entries[i].partition_id != JLOS_PARTITION_TYPE_FAT32_LBA_INT113) {
            continue;
        }
        jlos_partition_dev_create(&s_root_partition, ata_dev, entries[i].start_lba, entries[i].length);
        jlos_vfs_mount_t *mnt = jlos_vfs_mount("fat32", &s_root_partition.base, "/");
        if (!mnt) {
            printk_err("failed to mount rootfs on partition %d\n", i);
            return;
        }
        printk_debug("rootfs mounted on partition %d, type 0x%x\n", i, entries[i].partition_id);
        return;
    }
    printk_err("no FAT32 partition found\n");
}

JLOS_INITCALL(JLOS_INITCALL_LATE, jlos_rootfs_init);
