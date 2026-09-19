#include <hal/pci.h>
#include <kernel/initcall.h>
#include <kernel/memory_manager.h>

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

void jlos_hal_pci_config_write16(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint16_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t dword = jlos_hal_pci_config_read32(self, bus, dev, func, aligned);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    dword &= ~((uint32_t)0xFFFFu << shift);
    dword |=  ((uint32_t)val << shift);
    jlos_hal_pci_config_write32(self, bus, dev, func, aligned, dword);
}

void jlos_hal_pci_config_write8(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint8_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t dword = jlos_hal_pci_config_read32(self, bus, dev, func, aligned);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    dword &= ~((uint32_t)0xFFu << shift);
    dword |=  ((uint32_t)val << shift);
    jlos_hal_pci_config_write32(self, bus, dev, func, aligned, dword);
}

static void pci_subsys_init(void)
{
    jlos_hal_pci_init();
    jlos_memory_manager_switch_low();
    jlos_hal_pci_enumerate_and_bind_drivers();
    jlos_memory_manager_switch_main();
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, pci_subsys_init);
