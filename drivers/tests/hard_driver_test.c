#include <kernel/test.h>
#include <drivers/ata.h>
#include <hal/block.h>
#include <fs/partition.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_ata"
#include <kernel/printk.h>

#define ATA_SECTOR_SIZE   512
#define ATA_TEST_PATTERN  0xA5

JLOS_TEST(ata, identify)
{
    jlos_hal_block_dev_t *dev = jlos_ata_get_primary_dev();
    JLOS_ASSERT_NOT_NULL(dev);
    JLOS_TEST_NE(dev->total_sectors, 0);
}

JLOS_TEST(ata, read_mbr)
{
    jlos_hal_block_dev_t *dev = jlos_ata_get_primary_dev();
    JLOS_ASSERT_NOT_NULL(dev);
    uint8_t mbr[ATA_SECTOR_SIZE];
    JLOS_ASSERT_TRUE(jlos_hal_block_read(dev, 0, mbr, 1) >= 0);
    JLOS_TEST_EQ(mbr[510], 0x55);
    JLOS_TEST_EQ(mbr[511], 0xAA);
}

JLOS_TEST(ata, write_read_roundtrip)
{
    jlos_hal_block_dev_t *dev = jlos_ata_get_primary_dev();
    JLOS_ASSERT_NOT_NULL(dev);
    uint64_t test_lba = dev->total_sectors - 1;
    uint8_t orig[ATA_SECTOR_SIZE], wr[ATA_SECTOR_SIZE], rd[ATA_SECTOR_SIZE];
    JLOS_ASSERT_TRUE(jlos_hal_block_read(dev, test_lba, orig, 1) >= 0);
    for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
        wr[i] = (uint8_t)(i ^ ATA_TEST_PATTERN);
    }
    JLOS_TEST_TRUE(jlos_hal_block_write(dev, test_lba, wr, 1) >= 0);
    dev->ops->flush(dev);
    JLOS_TEST_TRUE(jlos_hal_block_read(dev, test_lba, rd, 1) >= 0);
    for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
        JLOS_TEST_EQ(rd[i], wr[i]);
    }
    jlos_hal_block_write(dev, test_lba, orig, 1);
    dev->ops->flush(dev);
}
