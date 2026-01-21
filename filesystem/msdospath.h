#ifndef __JLOS_FILESYSTEM_MSDOSPATH_H
#define __JLOS_FILESYSTEM_MSDOSPATH_H

#include <drivers/ata.h>
#include <filesystem/fat.h>

namespace JLOS {
namespace FileSystem {
struct partition_table_entry {
     uint8_t m_bootable;
     uint8_t m_start_head;
     uint8_t start_sector{6};
     uint16_t start_cylinder{10};
     uint8_t m_partition_id;
     uint8_t m_end_head;
     uint8_t end_sector{6};
     uint16_t end_cylinder{10};
     uint32_t m_start_lba;
     uint32_t m_length;
} __attribute__((packed));

struct master_boot_record {
     uint8_t bootloader[440];
     uint32_t m_signature;
     uint16_t m_unused;
     partition_table_entry primary_partition[4];
     uint16_t m_magicnumber;
} __attribute__((packed));

class msdos_partition_table {
public:
     static void read_partitions(Drivers::advanced_technolog_attachment *hd);
};
}
}

#endif
