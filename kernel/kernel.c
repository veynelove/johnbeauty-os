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

#if KERNEL_CONFIG_ENABLE_TESTS
#include <tools/tests/memory_te.h>
#include <tools/tests/multitask_te.h>
#include <tools/tests/hard_driver_te.h>
#include <tools/tests/http_server_te.h>
#include <tools/tests/udp_server_te.h>
#endif

#if KERNEL_CONFIG_DEBUG_CONSOLE
#include <tools/samples/debug_console.h>
#endif

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
    jlos_printk_init();
    jlos_hal_arch_init();
    printf("princess yihan is safe and happy!\n");

    jlos_mmu_t *mmu = jlos_mmu_get_kernel();
    jlos_mmu_init();
    jlos_arch_tss_init(jlos_mmu_data_selector(mmu));

    jlos_device_init(multiboot_structure);
    jlos_page_frame_allocator_init(VIRT_TO_PHYS(kernel_end));
    jlos_paging_initialize_kernel_paging();
    printf("paging initialized\n");

    uint8_t* low_memory_heap = (uint8_t*)PHYS_TO_VIRT(KERNEL_LOW_MEMORY_ADDR_START);
    jlos_memory_manager_t low_memory_manager_;
    jlos_memory_manager_init(&low_memory_manager_, low_memory_heap, KERNEL_LOW_MEMORY_SIZE);
    
    jlos_memory_manager_t memory_manager_;
    jlos_memory_manager_init_main(&memory_manager_);

    jlos_task_manager_t task_manager_;
    jlos_task_manager_init(&task_manager_);

    jlos_irq_manager_t irq_mgr;
    jlos_irq_manager_init(&irq_mgr, KERNEL_FIRST_INTERRUPT_VECTOR, mmu, &task_manager_);
    printf("interrupt manager initialized\n");

    jlos_syscall_handler_t syscalls;
    jlos_syscall_handler_init(&syscalls, &irq_mgr, 0x80);

    printf("initializing hardware, stage 1 start\n");
    jlos_driver_manager_t driver_manager_;
    jlos_driver_manager_init(&driver_manager_);

#if KERNEL_CONFIG_DEBUG_CONSOLE
    debug_console_init(&irq_mgr, &driver_manager_);
#endif

    jlos_hal_pci_controller_t pci_controller;
    jlos_hal_pci_init(&pci_controller);

    jlos_memory_manager_t *old_manager = jlos_active_memory_manager;
    jlos_active_memory_manager = &low_memory_manager_;
    printf("switched to low memory manager for PCI driver allocation\n");

    jlos_hal_pci_enumerate_and_bind_drivers(&pci_controller, &driver_manager_, &irq_mgr);
    jlos_active_memory_manager = old_manager;
    printf("switched back to main memory manager\n");
    printf("initializing hardware, stage 2 start\n");
    jlos_driver_manager_activate_all(&driver_manager_);

    printf("initializing hardware, stage 3 start\n");

    jlos_hal_timer_start_periodic(JLOS_HAL_TIME_FREQ_HZ);
    printf("PIT timer initialized\n");

    jlos_irq_manager_activate(&irq_mgr);
    printf("interrupts activated\n");

#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("Initializing network stack...\n");
#endif
    network_stack_t *network_stack = (network_stack_t *)jlos_malloc(sizeof(network_stack_t));
    network_init(network_stack, &driver_manager_);

#if KERNEL_CONFIG_ENABLE_TESTS
    printf("running tests...\n");
    memory_manager_test(multiboot_structure);
    multitask_test(mmu, &task_manager_);
    hard_driver_test();
    http_server_test(&network_stack->tcp);
    udp_server_test(&network_stack->udp);
#endif

    for (;;) {
        jlos_hal_halt();
    }
}