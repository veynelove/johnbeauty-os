#include <filesystem/fat.h>

extern void printf(const char *str);
extern void printf_hex(uint8_t);

void jlos_read_bios_block(jlos_ata_t *hd, uint32_t partition_offset)
{
    jlos_bios_parameter_block32_t bpb;
    jlos_ata_read28(hd, partition_offset, (uint8_t *)&bpb, sizeof(jlos_bios_parameter_block32_t));
    
    printf("sector per cluster: ");
    printf_hex(bpb.sectors_per_cluster);
    printf("\n");
    uint32_t fat_start = partition_offset + bpb.reserved_sectors;
    uint32_t fat_size = bpb.table_size;
    uint32_t data_start = fat_start + fat_size * bpb.fat_copies;
    uint32_t root_start = data_start + bpb.sectors_per_cluster * (bpb.root_cluster - 2);
    jlos_directory_entry_fat32_t dirent[16];
    jlos_ata_read28(hd, root_start, (uint8_t *)&dirent[0], 16 * sizeof(jlos_directory_entry_fat32_t));
    for (int i = 0; i < 16; i++) {
        if (dirent[i].name[0] == 0x00) {
            break;
        }
        if ((dirent[i].attributes & 0x0F) == 0x0F) {
            continue;
        }
        char foo[9];
        for (int j = 0; j < 8; j++) {
            foo[j] = dirent[i].name[j];
        }
        foo[8] = '\n';
        printf(foo);
        if ((dirent[i].attributes & 0x10) == 0x10) {
            continue;
        }
        uint32_t first_file_cluster = ((uint32_t)dirent[i].first_cluster_hi) << 16
            | ((uint32_t)dirent[i].first_cluster_low);

        int32_t SIZE = dirent[i].size;
        int32_t next_file_cluster = first_file_cluster;
        
        while (SIZE > 0) {
            uint32_t file_sector = data_start + bpb.sectors_per_cluster * (next_file_cluster - 2);
            int sector_offset = 0;
            uint8_t buffer[513];
            uint8_t fatbuffer[513];

            for (; SIZE > 0; SIZE -= 512) {
                jlos_ata_read28(hd, file_sector + sector_offset, buffer, 512);
                buffer[(SIZE > 512 ? 512 : SIZE)] = '\0';
                printf((char *)buffer);
                if (++sector_offset > bpb.sectors_per_cluster - 1) {
                    break;
                }
            }
            uint32_t fat_sector_for_current_cluster = next_file_cluster / (512 / sizeof(uint32_t));
            jlos_ata_read28(hd, fat_start + fat_sector_for_current_cluster, fatbuffer, 512);
            uint32_t fat_offset_in_sector_for_current_cluster = next_file_cluster % (512 / sizeof(uint32_t));
            next_file_cluster = ((uint32_t *)&fatbuffer)[fat_offset_in_sector_for_current_cluster] & 0x0FFFFFFF;
        }
    }
}