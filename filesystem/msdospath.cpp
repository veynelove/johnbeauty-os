#include <filesystem/msdospath.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printfHex(uint8_t);
}

namespace JLOS {
namespace FileSystem {
void MSDOSPartitionTable::ReadPartitions(Drivers::AdvancedTechnologAttachment *hd)
{
     MasterBootRecord mbr;
     hd->Read28(0, (uint8_t *)&mbr, sizeof(MasterBootRecord));
     // Kernel::printf("MBR: ");
     // for (int i = 0x1BE; i <= 0x1FF; i++) {
     //      Kernel::printfHex(((uint8_t *)&mbr)[i]);
     //      Kernel::printf(" ");
     // }
     Kernel::printf("\n");
     if (mbr.magicnumber != 0xAA55) {
          Kernel::printf("illegal MBR");
          return;
     }
     for (int i = 0; i < 4; i++) {
          if (mbr.primaryPartition[i].partition_id == 0) {
               continue;
          }
          Kernel::printf(" Partition ");
          Kernel::printfHex(i & 0xFF);
          if (mbr.primaryPartition[i].bootable == 0x80) {
               Kernel::printf(" booable. Type");
          } else {
               Kernel::printf(" not bootable. Type ");
          }
          Kernel::printfHex(mbr.primaryPartition[i].partition_id);

          ReadBiosBlock(hd, mbr.primaryPartition[i].start_lba);
     }
}
}
}
