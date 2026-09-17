#include <hal/pci.h>
#include <hal/diag.h>
#include <kernel/initcall.h>
#include <kernel/memory_manager.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

void jlos_hal_pci_init(void)
{ 
    jlos_pci_controller_init();
}

uint32_t jlos_hal_pci_config_read32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t cfg_addr = 0x80000000u
        | ((uint32_t)(bus  & 0xFFu)  << 16u)
        | ((uint32_t)(dev  & 0x1Fu)  << 11u)
        | ((uint32_t)(func & 0x07u)  <<  8u)
        | ((uint32_t)aligned & 0xFCu);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR32, 0xCF8, cfg_addr);
    (void)cfg_addr;
    uint32_t v = jlos_pci_controller_read(self, bus, dev, func, aligned);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_RD32, 0xCFC, v);
    return v;
}

uint16_t jlos_hal_pci_config_read16(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg)
{
    uint32_t dword = jlos_hal_pci_config_read32(self, bus, dev, func, reg);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    return (uint16_t)((dword >> shift) & 0xFFFFu);
}

uint8_t jlos_hal_pci_config_read8(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg)
{
    uint32_t dword = jlos_hal_pci_config_read32(self, bus, dev, func, reg);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    return (uint8_t)((dword >> shift) & 0xFFu);
}

void jlos_hal_pci_config_write32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint32_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t cfg_addr = 0x80000000u
        | ((uint32_t)(bus  & 0xFFu)  << 16u)
        | ((uint32_t)(dev  & 0x1Fu)  << 11u)
        | ((uint32_t)(func & 0x07u)  <<  8u)
        | ((uint32_t)aligned & 0xFCu);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR32, 0xCF8, cfg_addr);
    (void)cfg_addr;
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR32, 0xCFC, val);
    jlos_pci_controller_write(self, bus, dev, func, aligned, val);
}

void jlos_hal_pci_config_write16(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint16_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t dword = jlos_pci_controller_read(self, bus, dev, func, aligned);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    dword &= ~((uint32_t)0xFFFFu << shift);
    dword |=  ((uint32_t)val << shift);
    jlos_pci_controller_write(self, bus, dev, func, aligned, dword);
}

void jlos_hal_pci_config_write8(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint8_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t dword = jlos_pci_controller_read(self, bus, dev, func, aligned);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    dword &= ~((uint32_t)0xFFu << shift);
    dword |=  ((uint32_t)val << shift);
    jlos_pci_controller_write(self, bus, dev, func, aligned, dword);
}

void jlos_hal_pci_enumerate_and_bind_drivers(void)
{
    jlos_pci_controller_select_drivers(g_driver_manager_ptr, (jlos_interrupt_manager_t *)jlos_active_irq_manager);
}


jlos_hal_pci_device_t jlos_hal_pci_get_device_descriptor(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func)
{ 
    return jlos_pci_controller_get_device_descriptor(self, bus, dev, func);
}

jlos_hal_pci_bar_t jlos_hal_pci_get_bar(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t bar_index)
{ 
    return jlos_pci_controller_get_base_address_register(self, bus, dev, func, bar_index);
}

static void pci_subsys_init(void)
{
    jlos_hal_pci_init();
    jlos_memory_manager_switch_low();
    jlos_hal_pci_enumerate_and_bind_drivers();
    Jlos_memory_manager_switch_main();
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, pci_subsys_init);

#endif
