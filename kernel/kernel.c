#include <hal/irq.h>
#include <hal/io.h>
#include <hal/pci.h>
#include <hal/mmu.h>
#include <hal/syscall.h>
#include <hal/timer.h>
#include <hal/spinlock.h>
#include <hal/hal.h>
#include <hal/serial.h>
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

static jlos_spinlock_t s_printf_lock = JLOS_SPINLOCK_INIT;

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

void printf_scroll_screen()
{
    static uint16_t* video_memory = (uint16_t *)0xb8000;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            video_memory[80 * y + x] = video_memory[80 * (y + 1) + x];
        }
    }
    for (int x = 0; x < 80; x++) {
        video_memory[80 * 24 + x] = (video_memory[80 * 24 + x] & 0xFF00) | ' ';
    }
}

void printf(const char* str)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    jlos_hal_serial_default_puts(str);
    static uint16_t* video_memory = (uint16_t *)0xb8000;
    static uint8_t m_x = 0, m_y = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        switch (str[i]) {
            case '\n': m_y++; m_x = 0; break;
            default:
                video_memory[80 * m_y + m_x] = (video_memory[80 * m_y + m_x] & 0xFF00) | str[i];
                m_x++;
        }
        if (m_x >= 80) {
            m_y++;
            m_x = 0;
        }
        if (m_y >= 25) {
            printf_scroll_screen();
            m_y = 24;
            m_x = 0;
        }
    }
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_hex(uint8_t key)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    char foo[3] = "00";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0F];
    foo[1] = hex[key & 0x0F];
    printf(foo);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_hex16(uint16_t value)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    char foo[5] = "0000";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(value >> 12) & 0x0F];
    foo[1] = hex[(value >> 8) & 0x0F];
    foo[2] = hex[(value >> 4) & 0x0F];
    foo[3] = hex[value & 0x0F];
    printf(foo);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_hex32(uint32_t value)
{
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    char foo[9] = "00000000";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(value >> 28) & 0x0F];
    foo[1] = hex[(value >> 24) & 0x0F];
    foo[2] = hex[(value >> 20) & 0x0F];
    foo[3] = hex[(value >> 16) & 0x0F];
    foo[4] = hex[(value >> 12) & 0x0F];
    foo[5] = hex[(value >> 8) & 0x0F];
    foo[6] = hex[(value >> 4) & 0x0F];
    foo[7] = hex[value & 0x0F];
    printf(foo);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void printf_char(char c) {
    uint32_t flags = jlos_spin_lock_irqsave(&s_printf_lock);
    char buf[2] = {c, '\0'};
    printf(buf);
    jlos_spin_unlock_irqrestore(&s_printf_lock, flags);
}

void sysprintf(char *str)
{
    __asm__ __volatile__("int $0x80" : : "a" (4), "b" (str));
}

typedef void (*constructor)();
extern constructor __init_array_start;
extern constructor __init_array_end;

void call_constructors()
{
    for (constructor* i = &__init_array_start; i != &__init_array_end; i++) {
        (*i)();
    }
}

void john_beauty_main(const multiboot_info_t *multiboot_structure, uint32_t m_magicnumber)
{
    jlos_spinlock_init(&s_printf_lock);
    jlos_hal_serial_default_init();
    jlos_hal_arch_init();
    printf("princess yihan is safe and happy!\n");

    jlos_mmu_t mmu_ctx;
    jlos_mmu_init(&mmu_ctx);
    
    uint8_t* low_memory_heap = (uint8_t*)(0x50000);
    jlos_memory_manager_t low_memory_manager_;
    jlos_memory_manager_init(&low_memory_manager_, low_memory_heap, 0x50000);
    
    uint8_t* heap_start = (uint8_t*)(1024 * (multiboot_structure->mem_upper - 1024 * 16));
    jlos_memory_manager_t memory_manager_;
    jlos_memory_manager_init(&memory_manager_, heap_start, 1024 * 1024 * 16);
    
    jlos_task_manager_t task_manager_;
    jlos_task_manager_init(&task_manager_);
    
    jlos_irq_manager_t irq_mgr;
    jlos_irq_manager_init(&irq_mgr, 0x20, &mmu_ctx, &task_manager_);
    
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
    /* 避免栈溢出：network_stack_t 很大，改到静态存储区或堆上 */
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
        
    }
}