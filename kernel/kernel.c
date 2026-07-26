#include <hal/irq.h>
#include <hal/io.h>
#include <hal/pci.h>
#include <hal/mmu.h>
#include <hal/syscall.h>
#include <hal/timer.h>
#include <hal/hal.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <drivers/amd_am79c973.h>
#include <net/network.h>
#include <filesystem/msdospath.h>
#include <filesystem/fat.h>
#include <common/multiboot.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>

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

void john_beauty_main(const multiboot_info_t *multiboot_structure, uint32_t m_magicnumber, uint32_t kernel_end)
{
    jlos_printk_init();
    jlos_hal_arch_init();
    printf("princess yihan is safe and happy!\n");

    jlos_mmu_t mmu_ctx;
    jlos_mmu_init(&mmu_ctx);
    
    jlos_page_frame_allocator_init(KERNEL_MEMORY_ADDR_START, KERNEL_MEMORY_ADDR_END, kernel_end);
    printf("page frame allocator initialized\n");
    jlos_paging_initialize_kernel_paging();
    printf("paging initialized\n");
    
    uint8_t* low_memory_heap = (uint8_t*)(0x50000);
    jlos_memory_manager_t low_memory_manager_;
    jlos_memory_manager_init(&low_memory_manager_, low_memory_heap, 0x50000);
    
    void *first_free_frame_ptr = jlos_page_frame_malloc();
    jlos_page_frame_free(first_free_frame_ptr);
    uint8_t* heap_start = (uint8_t*)(first_free_frame_ptr);
    jlos_memory_manager_t memory_manager_;
    jlos_memory_manager_init(&memory_manager_, heap_start, 16 * 1024 * 1024);
    
    jlos_task_manager_t task_manager_;
    jlos_task_manager_init(&task_manager_);
    
    jlos_irq_manager_t irq_mgr;
    jlos_irq_manager_init(&irq_mgr, 0x20, &mmu_ctx, &task_manager_);
    printf("interrupt manager initialized\n");

    jlos_syscall_t syscalls;
    jlos_syscall_init(&syscalls, &irq_mgr, 0x80);

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

    jlos_hal_timer_start_periodic(100);
    printf("PIT timer initialized (100Hz)\n");

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
    multitask_test(&mmu_ctx, &task_manager_);
    hard_driver_test();
    http_server_test(&network_stack->tcp);
    udp_server_test(&network_stack->udp);
#endif
    
    for (;;) {
        jlos_hal_halt();
    }
}