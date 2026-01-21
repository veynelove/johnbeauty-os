#include <filesystem/msdospath.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printf_hex(uint8_t);
}

namespace JLOS {
namespace FileSystem {
void msdos_partition_table::read_partitions(Drivers::advanced_technolog_attachment *hd)
{
     master_boot_record mbr;
     hd->read28(0, (uint8_t *)&mbr, sizeof(master_boot_record));
     // Kernel::printf("MBR: ");
     // for (int i = 0x1BE; i <= 0x1FF; i++) {
     //      Kernel::printf_hex(((uint8_t *)&mbr)[i]);
     //      Kernel::printf(" ");
     // }
     Kernel::printf("\n");
     if (mbr.m_magicnumber != 0xAA55) {
          Kernel::printf("illegal MBR");
          return;
     }
     for (int i = 0; i < 4; i++) {
          if (mbr.primary_partition[i].m_partition_id == 0) {
               continue;
          }
          Kernel::printf(" partition ");
          Kernel::printf_hex(i & 0xFF);
          if (mbr.primary_partition[i].m_bootable == 0x80) {
               Kernel::printf(" booable. m_type");
          } else {
               Kernel::printf(" not m_bootable. m_type ");
          }
          Kernel::printf_hex(mbr.primary_partition[i].m_partition_id);

          read_bios_block(hd, mbr.primary_partition[i].m_start_lba);
     }
}
}
}
