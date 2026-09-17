#include <common/multiboot.h>
#include <hal/irq.h>
#include <hal/io.h>
#include <hal/mmu.h>
#include <hal/hal.h>
#include <hal/context.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/device.h>
#include <kernel/initcall.h>

#if KERNEL_CONFIG_ENABLE_TESTS
#include <tools/tests/memory_te.h>
#include <tools/tests/multitask_te.h>
#include <tools/tests/pfa_te.h>
#include <tools/tests/paging_te.h>
#include <tools/tests/hard_driver_te.h>
#include <tools/tests/http_server_te.h>
#include <tools/tests/udp_server_te.h>
#endif

#define JLOS_KERNEL_LOG_SUBSYS "boot"

void john_beauty_main(const multiboot_info_t *multiboot_structure, uint32_t kernel_end)
{
    jlos_hal_arch_init();
    jlos_printk_init();
    printk_info("princess yihan is safe and happy!\n");

    jlos_mmu_init();
    jlos_arch_tss_init();

    jlos_device_init(multiboot_structure);
    jlos_pfa_boot_alloc_init(VIRT_TO_PHYS(kernel_end));
    jlos_paging_initialize_kernel_paging(jlos_pfa_boot_alloc_page);
    jlos_page_frame_allocator_init();
    printk_info("paging initialized\n");

    jlos_do_initcalls();
    
#if KERNEL_CONFIG_ENABLE_TESTS
    printk_info("=== running tests ===\n");
    memory_manager_test(multiboot_structure);
    pfa_test();
    paging_test();
    multitask_test(jlos_mmu_get_kernel(), g_task_manager_ptr);
    hard_driver_test();
    http_server_test();
    udp_server_test();
#endif

    for (;;) {
        jlos_hal_halt();
    }
}