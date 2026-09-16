#include <common/multiboot.h>
#include <hal/irq.h>
#include <hal/io.h>
#include <hal/pci.h>
#include <hal/mmu.h>
#include <hal/timer.h>
#include <hal/hal.h>
#include <hal/context.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/ata.h>
#include <drivers/amd_am79c973.h>
#include <net/network.h>
#include <filesystem/msdospath.h>
#include <filesystem/fat.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/syscall.h>
#include <kernel/device.h>
#include <kernel/console.h>

#if KERNEL_CONFIG_ENABLE_TESTS
#include <tools/tests/memory_te.h>
#include <tools/tests/multitask_te.h>
#include <tools/tests/pfa_te.h>
#include <tools/tests/paging_te.h>
#include <tools/tests/hard_driver_te.h>
#include <tools/tests/http_server_te.h>
#include <tools/tests/udp_server_te.h>
#endif

#if KERNEL_CONFIG_DEBUG_CONSOLE
#include <tools/samples/debug_console.h>
#endif

#define JLOS_KERNEL_LOG_SUBSYS "boot"

typedef void (*constructor)();
extern constructor __init_array_start;
extern constructor __init_array_end;

void call_constructors()
{
    for (constructor* i = &__init_array_start; i != &__init_array_end; i++) {
        (*i)();
    }
}

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

    jlos_hal_arch_display_init_fb();
    jlos_console_reinit();
    printk_info("framebuffer console enabled\n");

    jlos_memory_manager_init();

    jlos_task_manager_init();

    jlos_irq_manager_init();
    printk_info("interrupt manager initialized\n");

    jlos_syscall_handler_init();
    jlos_driver_manager_init();

#if KERNEL_CONFIG_DEBUG_CONSOLE
    debug_console_init();
#endif

    jlos_hal_pci_init();

    jlos_memory_manager_switch_low();
    printk_info("switched to low memory manager for PCI driver allocation\n");

    jlos_hal_pci_enumerate_and_bind_drivers();
    
    Jlos_memory_manager_switch_main();
    printk_info("switched back to main memory manager\n");
    
    jlos_driver_manager_activate_all();

    jlos_hal_timer_start_periodic(JLOS_HAL_TIME_FREQ_HZ);
    printk_info("PIT timer initialized\n");

    jlos_irq_manager_activate();
    printk_info("interrupts activated\n");

#if KERNEL_CONFIG_DEBUG_NETWORK
    printk_info("initializing network stack...\n");
#endif
    network_init();

#if KERNEL_CONFIG_ENABLE_TESTS
    printk_info("=== running tests ===\n");
    memory_manager_test(multiboot_structure);
    pfa_test();
    paging_test();
    multitask_test(jlos_mmu_get_kernel(), g_task_manager_ptr);
    hard_driver_test();
    http_server_test(&g_network_stack->tcp);
    udp_server_test(&g_network_stack->udp);
#endif

    for (;;) {
        jlos_hal_halt();
    }
}