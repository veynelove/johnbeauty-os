#include <filesystem/msdospath.h>
#include <filesystem/fat.h>
#include <kernel/printk.h>

void jlos_msdos_partition_table_read_partitions(jlos_ata_t *hd)
{
    jlos_master_boot_record_t mbr;
    jlos_ata_read28(hd, 0, (uint8_t *)&mbr, sizeof(jlos_master_boot_record_t));
    printf("\n");
    if (mbr.m_magicnumber != 0xAA55) {
        printf("illegal MBR");
        return;
    }
    for (int i = 0; i < 4; i++) {
        if (mbr.primary_partition[i].m_partition_id == 0) {
            continue;
        }
        printf(" partition ");
        printf_hex(i & 0xFF);
        if (mbr.primary_partition[i].m_bootable == 0x80) {
            printf(" booable. m_type");
        } else {
            printf(" not m_bootable. m_type ");
        }
        printf_hex(mbr.primary_partition[i].m_partition_id);
        jlos_read_bios_block(hd, mbr.primary_partition[i].m_start_lba);
    }
}