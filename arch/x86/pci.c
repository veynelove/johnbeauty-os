#include <arch/x86/pci.h>
#include <hal/diag.h>
#include <drivers/driver.h>
#include <drivers/amd_am79c973.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "pci"
#include <kernel/printk.h>

static jlos_hal_pci_controller_t s_pci_controller;

void jlos_hal_pci_init(void)
{
    jlos_port_io32_init(&s_pci_controller.data_port, 0xCFC);
    jlos_port_io32_init(&s_pci_controller.command_port, 0xCF8);
}

uint32_t jlos_hal_pci_config_read32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t cfg_addr = JLOS_PCI_CFG_ADDR_ENABLE
        | ((uint32_t)(bus  & 0xFFu)  << 16u)
        | ((uint32_t)(dev  & 0x1Fu)  << 11u)
        | ((uint32_t)(func & 0x07u)  <<  8u)
        | ((uint32_t)aligned & 0xFCu);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR32, 0xCF8, cfg_addr);
    jlos_port_io32_write(&self->command_port, cfg_addr);
    uint32_t v = jlos_port_io32_read(&self->data_port);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_RD32, 0xCFC, v);
    return v;
}

void jlos_hal_pci_config_write32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint32_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t cfg_addr = JLOS_PCI_CFG_ADDR_ENABLE
        | ((uint32_t)(bus  & 0xFFu)  << 16u)
        | ((uint32_t)(dev  & 0x1Fu)  << 11u)
        | ((uint32_t)(func & 0x07u)  <<  8u)
        | ((uint32_t)aligned & 0xFCu);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR32, 0xCF8, cfg_addr);
    jlos_port_io32_write(&self->command_port, cfg_addr);
    HAL_TRACE_IO(JLOS_HAL_TRACE_OP_WR32, 0xCFC, val);
    jlos_port_io32_write(&self->data_port, val);
}

static bool pci_device_has_functions(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev)
{
    return jlos_hal_pci_config_read8(self, bus, dev, 0, 0x0E) & (1 << 7);
}

static jlos_driver_t *pci_network_controller_handle(jlos_hal_pci_controller_t *self, jlos_hal_pci_device_t dev, jlos_irq_manager_t *interrupts)
{
    jlos_driver_t *driver = NULL;
    switch (dev.vendor_id) {
        case 0x1022 : {
            uint16_t command = jlos_hal_pci_config_read16(self, dev.bus, dev.device, dev.function, 0x04);
            command |= 0x07;
            jlos_hal_pci_config_write16(self, dev.bus, dev.device, dev.function, 0x04, command);
            switch (dev.device_id) {
                case 0x2000 : {
                    printk_debug("amd am79c973 pci command: 0x%x\n", command);
                    printk_debug("allocating amd am79c973 driver structure\n");
                    driver = (jlos_driver_t *)jlos_kalloc(sizeof(jlos_amd_am79c973_t));
                    if (driver) {
                        printk_debug("amd am79c9973 driver allocated at: 0x%x\n", (uint32_t)driver);
                        jlos_amd_am79c973_init((jlos_amd_am79c973_t *)driver, &dev, interrupts);
                        return driver;
                    }
                    printk_err("amd am79c973 allocation failed\n");
                    break;
                } 
            }
            break;
        }
        case 0x8086 :
            break;
    }
    printk_warn("no network driver\n");
    return driver;
}

static jlos_driver_t *pci_get_driver(jlos_hal_pci_controller_t *self, jlos_hal_pci_device_t dev, jlos_irq_manager_t *interrupts)
{
    switch (dev.class_id) {
        case 0x02:
            return pci_network_controller_handle(self, dev, interrupts);
        default:
            break;
    }
    return NULL;
}

jlos_hal_pci_device_t jlos_hal_pci_get_device_descriptor(jlos_hal_pci_controller_t* self, uint16_t bus, uint16_t dev, uint16_t func)
{
    jlos_hal_pci_device_t result;
    result.bus = bus;
    result.device = dev;
    result.function = func;

    result.vendor_id = jlos_hal_pci_config_read16(self, bus, dev, func, 0x00);
    result.device_id = jlos_hal_pci_config_read16(self, bus, dev, func, 0x02);

    result.class_id = jlos_hal_pci_config_read8(self, bus, dev, func, 0x0b);
    result.subclass_id = jlos_hal_pci_config_read8(self, bus, dev, func, 0x0a);
    result.interface_id = jlos_hal_pci_config_read8(self, bus, dev, func, 0x09);

    result.revision = jlos_hal_pci_config_read8(self, bus, dev, func, 0x08);
    result.interrupt = jlos_hal_pci_config_read8(self, bus, dev, func, 0x3c);

    return result; 
}

jlos_hal_pci_bar_t jlos_hal_pci_get_base_address_register(jlos_hal_pci_controller_t* self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t bar)
{
    jlos_hal_pci_bar_t result = {false, NULL, 0, JLOS_HAL_PCI_MEMORY_MAPPING};
    uint32_t headertype = jlos_hal_pci_config_read8(self, bus, dev, func, 0x0E) & 0x7F;
    int maxbars = 6 - (4 * headertype);
    if (bar >= (uint16_t)maxbars) {
        return result;
    }
    uint32_t bar_value = jlos_hal_pci_config_read32(self, bus, dev, func, 0x10 + 4 * bar);
    result.type = (bar_value & 0x1) ? JLOS_HAL_PCI_INPUT_OUTPUT : JLOS_HAL_PCI_MEMORY_MAPPING;
    
    if (bar_value == 0) {
        return result;
    }
    
    jlos_hal_pci_config_write32(self, bus, dev, func, 0x10 + 4 * bar, 0xFFFFFFFF);
    uint32_t size_value = jlos_hal_pci_config_read32(self, bus, dev, func, 0x10 + 4 * bar);
    
    if (result.type == JLOS_HAL_PCI_INPUT_OUTPUT) {
        size_value &= ~0x3;
        uint32_t bar_size = (~size_value) + 1;
        result.size = bar_size;
        jlos_hal_pci_config_write32(self, bus, dev, func, 0x10 + 4 * bar, bar_value);
        result.address = (uint8_t *)(bar_value & ~0x3);
        result.prefetchable = false;
    } else {
        size_value &= ~0xF;
        uint32_t bar_size = (~size_value) + 1;
        result.size = bar_size;
        jlos_hal_pci_config_write32(self, bus, dev, func, 0x10 + 4 * bar, bar_value);
        result.address = (uint8_t *)(bar_value & ~0xF);
        result.prefetchable = (bar_value & 0x8) != 0;
    }
    return result;
}

void jlos_hal_pci_enumerate_and_bind_drivers(void)
{
    jlos_hal_pci_controller_t *self = &s_pci_controller;
    for (int bus = 0; bus < 8; bus++) {
        for (int device = 0; device < 32; device++) {
            int num_functions = pci_device_has_functions(self, bus, device) ? 8 : 1;
            for (int function = 0; function < num_functions; function++) {
                jlos_hal_pci_device_t dev = jlos_hal_pci_get_device_descriptor(self, bus, device, function);
                if (dev.vendor_id == 0x0000 || dev.vendor_id == 0xFFFF) {
                    continue;
                }
                for (int bar_num = 0; bar_num < 6; bar_num++) {
                    jlos_hal_pci_bar_t bar = jlos_hal_pci_get_base_address_register(self, bus, device, function, bar_num);
                    if (bar.address && (bar.type == JLOS_HAL_PCI_INPUT_OUTPUT)) {
                        dev.port_base = (uint32_t)bar.address;
                    }
                }
                jlos_driver_t *driver = pci_get_driver(self, dev, jlos_active_irq_manager);
                if (driver != NULL) {
                    jlos_driver_manager_add_driver(g_driver_manager_ptr, driver);
                }
            }
        }
    }
}
