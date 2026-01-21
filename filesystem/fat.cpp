#include <filesystem/fat.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printf_hex(uint8_t);
}

namespace JLOS {
namespace FileSystem {
void read_bios_block(Drivers::advanced_technolog_attachment *hd, uint32_t partition_offset)
{
     bios_parameter_block32 bpb;
     hd->read28(partition_offset, (uint8_t *)&bpb, sizeof(bios_parameter_block32));
     
     Kernel::printf("sector per cluster: ");
     Kernel::printf_hex(bpb.m_sectors_per_cluster);
     Kernel::printf("\n");
     uint32_t fat_start = partition_offset + bpb.m_reserved_sectors;
     uint32_t fat_size = bpb.m_table_size;
     uint32_t data_start = fat_start + fat_size * bpb.m_fat_copies;
     uint32_t root_start = data_start + bpb.m_sectors_per_cluster * (bpb.m_root_cluster - 2);
     directory_entry_fat32 dirent[16];
     hd->read28(root_start, (uint8_t *)&dirent[0], 16 * sizeof(directory_entry_fat32));
     for (int i = 0; i < 16; i++) {
          if (dirent[i].name[0] == 0x00) {
               break;
          }
          if ((dirent[i].m_attributes & 0x0F) == 0x0F) {
               continue;;
          }
          char *foo = "       \n";
          for (int j = 0; j < 8; j++) {
               foo[j] = dirent[i].name[j];
          }
          Kernel::printf(foo);
          if ((dirent[i].m_attributes & 0x10) == 0x10) { // directory
               continue;
          }
          uint32_t first_file_cluster = ((uint32_t)dirent[i].m_first_cluster_hi) << 16
               | ((uint32_t)dirent[i].m_first_cluster_low);

          int32_t SIZE = dirent[i].m_size;
          int32_t next_file_cluster = first_file_cluster;
          
          while (SIZE > 0) {
               uint32_t file_sector = data_start + bpb.m_sectors_per_cluster * (next_file_cluster - 2);
               int sector_offset = 0;
               uint8_t buffer[513];
               uint8_t fatbuffer[513];

               for (; SIZE > 0; SIZE -= 512) {
                    hd->read28(file_sector + sector_offset, buffer, 512);
                    buffer[(SIZE > 512 ? 512 : SIZE)] = '\0';
                    Kernel::printf((char *)buffer);
                    if (++sector_offset > bpb.m_sectors_per_cluster - 1) {
                         break;
                    }
               }
               uint32_t fat_sector_for_current_cluster = next_file_cluster / (512 / sizeof(uint32_t));
               hd->read28(fat_start + fat_sector_for_current_cluster, fatbuffer, 512);
               uint32_t fat_offset_in_sector_for_current_cluster = next_file_cluster % (512 / sizeof(uint32_t));
               next_file_cluster = ((uint32_t *)&fatbuffer)[fat_offset_in_sector_for_current_cluster] & 0x0FFFFFFF;
          }
     }
}
}
}
