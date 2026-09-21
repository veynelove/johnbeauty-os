#include <tools/tests/hard_driver_te.h>
#include <drivers/ata.h>
#include <filesystem/partition.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"
#include <kernel/printk.h>

void hard_driver_test()
{
    printk_info("ata test skipped (use simplified mode)\n");
}
