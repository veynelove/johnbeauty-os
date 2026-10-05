#include <drivers/tests/hard_driver_test.h>
#include <drivers/ata.h>
#include <hal/block.h>
#include <fs/partition.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "t_ata"
#include <kernel/printk.h>

#define ATA_SECTOR_SIZE 512
#define ATA_TEST_PATTERN 0xA5

static int test_identify(jlos_hal_block_dev_t *dev)
{
    int fail = 0;
    printk_info("[1] identify\n");
    if (dev->total_sectors == 0) {
        printk_err("total_sectors == 0\n");
        fail++;
    } else {
        printk_info("sectors=%u bps=%u\n",
               (unsigned)dev->total_sectors,
               dev->bytes_per_sector);
    }
    return fail;
}

static int test_read_mbr(jlos_hal_block_dev_t *dev)
{
    int fail = 0;
    printk_info("[2] read MBR (sector 0)\n");
    uint8_t mbr[ATA_SECTOR_SIZE];
    if (jlos_hal_block_read(dev, 0, mbr, 1) < 0) {
        printk_err("read sector 0 failed\n");
        fail++;
    } else if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
        printk_info("MBR signature 55AA\n");
    } else {
        printk_err("MBR signature %02x%02x\n", mbr[510], mbr[511]);
        fail++;
    }
    return fail;
}

static int test_write_read_roundtrip(jlos_hal_block_dev_t *dev)
{
    int fail = 0;
    printk_info("[3] write+read roundtrip (last sector)\n");
    uint64_t test_lba = dev->total_sectors - 1;
    uint8_t orig[ATA_SECTOR_SIZE], wr[ATA_SECTOR_SIZE], rd[ATA_SECTOR_SIZE];

    if (jlos_hal_block_read(dev, test_lba, orig, 1) < 0) {
        printk_err("save original sector %u failed\n", (unsigned)test_lba);
        fail++;
        return fail;
    }

    for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
        wr[i] = (uint8_t)(i ^ ATA_TEST_PATTERN);
    }

    if (jlos_hal_block_write(dev, test_lba, wr, 1) < 0) {
        printk_err("write test sector failed\n");
        fail++;
        goto restore;
    }
    dev->ops->flush(dev);

    if (jlos_hal_block_read(dev, test_lba, rd, 1) < 0) {
        printk_err("read back test sector failed\n");
        fail++;
        goto restore;
    }

    {
        int match = 1;
        for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
            if (rd[i] != wr[i]) {
                match = 0;
                break;
            }
        }
        if (!match) {
            printk_err("roundtrip data mismatch\n");
            fail++;
        } else {
            printk_info("roundtrip data match\n");
        }
    }

restore:
    jlos_hal_block_write(dev, test_lba, orig, 1);
    dev->ops->flush(dev);
    return fail;
}

void hard_driver_test(void)
{
    int fails = 0;
    printk_info("=== ata test start ===\n");

    jlos_hal_block_dev_t *dev = jlos_ata_get_primary_dev();
    if (!dev) {
        printk_err("no primary ATA device\n");
        fails++;
        goto done;
    }

    printk_info("dev: type=%u sectors=%u\n",
           dev->dev_type, (unsigned)dev->total_sectors);

    fails += test_identify(dev);
    fails += test_read_mbr(dev);
    fails += test_write_read_roundtrip(dev);

    if (!fails) {
        printk_info("ata: all passed\n");
    } else {
        printk_err("ata: %d failures\n", fails);
    }

done:
    ;
}

#if KERNEL_CONFIG_ENABLE_TESTS
JLOS_INITCALL(JLOS_INITCALL_TEST, hard_driver_test);
#endif
