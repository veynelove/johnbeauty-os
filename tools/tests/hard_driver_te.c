#include <tools/tests/hard_driver_te.h>
#include <drivers/ata.h>
#include <filesystem/msdospath.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "test"

void hard_driver_test()
{
    printk_info("ata test skipped (use simplified mode)\n");
}
