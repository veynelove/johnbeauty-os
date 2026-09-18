#include <hal/hal.h>
#include <hal/display.h>
#include <hal/device.h>
#include <hal/kernel_syscall.h>
#include <hal/hal_arch.h>

static jlos_device_t s_dev_pic_master = {
    .name       = "8259 PIC Master",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 0xFF },
    .resources  = {
        { JLOS_RES_IO_PORT, 0x20, 0x21, 0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_pic_slave = {
    .name       = "8259 PIC Slave",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 2 },
    .resources  = {
        { JLOS_RES_IO_PORT, 0xA0, 0xA1, 0 },
        { JLOS_RES_IRQ,     2,    2,    0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_pit_timer = {
    .name       = "8253 PIT Timer",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 0 },
    .resources  = {
        { JLOS_RES_IO_PORT, 0x40, 0x43, 0 },
        { JLOS_RES_IRQ,     0,    0,    0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_pci_cfg = {
    .name       = "PCI Config Mechanism #1",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 0xFF },
    .resources  = {
        { JLOS_RES_IO_PORT, 0xCF8, 0xCFF, 0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_uart_com1 = {
    .name       = "16550 UART COM1",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0x3F8, .mmio_size = 8, .irq = 4 },
    .resources  = {
        { JLOS_RES_IO_PORT, 0x3F8, 0x3FF, 0 },
        { 0 },
    },
    .drv = 0,
};

void jlos_hal_arch_init(void)
{
    jlos_hal_info_t *info = jlos_hal_info_get_for_init();
    info->cpu_model     = 0x00000686; 
    info->cpu_has_cpuid = true;
    info->cpu_has_apic  = true;
    
    info->irq_mode        = JLOS_HAL_IRQ_PIC_8259;
    info->irq_base_vector = 0x20;
    
    info->timer_mode           = JLOS_HAL_TIMER_PIT_8253;
    info->timer_input_clock_hz = 1193180ULL;
    
    info->pci_mmconfig_base   = 0;
    info->pci_ecam_available  = false;

    info->mmio_reserved_start = 0x000A0000;
    info->mmio_reserved_end   = 0xFFFFFFFF;
    
    jlos_hal_device_register(&s_dev_pic_master);
    jlos_hal_device_register(&s_dev_pic_slave);
    jlos_hal_device_register(&s_dev_pit_timer);
    jlos_hal_device_register(&s_dev_pci_cfg);
    jlos_hal_device_register(&s_dev_uart_com1);

    jlos_hal_arch_display_register();
    jlos_hal_kernel_segments_init();
    jlos_hal_arch_syscall_init();
}

void jlos_hal_arch_display_register(void)
{
}

void jlos_hal_halt(void)
{
    __asm__ __volatile__("hlt");
}

void jlos_hal_enable_interrupts(void)
{
    __asm__ __volatile__("sti");
}
