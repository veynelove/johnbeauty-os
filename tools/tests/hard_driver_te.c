#include <tools/tests/hard_driver_te.h>

#include <drivers/ata.h>
#include <filesystem/msdospath.h>

extern void printf(const char *);

void hard_driver_test()
{
    jlos_ata_t ata0m;
    jlos_ata_init(&ata0m, 0x1F0, true);
    printf("ATA primary m_master: ");
    jlos_ata_identify(&ata0m);
    jlos_ata_t ata0s;
    jlos_ata_init(&ata0s, 0x1F0, false);
    printf("ATA primary m_master: ");
    jlos_ata_identify(&ata0s);
    printf("\n\n\n\n\n\n\n\n\n\n");
    jlos_msdos_partition_table_read_partitions(&ata0s);

    jlos_ata_t ata1m;
    jlos_ata_init(&ata1m, 0x170, true);
    jlos_ata_t ata1s;
    jlos_ata_init(&ata1s, 0x170, false);
}
