#include <hdc/pci.h>
#include <drivers/driver.h>
#include <drivers/amd_am79c973.h>
#include <kernel/memory_manager.h>
#include <hdc/pci.h>

extern void printf(const char *str);
extern void printf_hex(uint8_t);
extern void printf_hex32(uint32_t);

void jlos_pci_controller_init(jlos_pci_controller_t* self)
{
    jlos_port32_bit_init(&self->m_data_port, 0xCFC);
    jlos_port32_bit_init(&self->m_command_port, 0xCF8);
}

void jlos_pci_controller_destroy(jlos_pci_controller_t* self)
{
}

uint32_t jlos_pci_controller_read(jlos_pci_controller_t* self, uint16_t m_bus, uint16_t m_device,
    uint16_t m_function, uint32_t registeroffset)
{
    uint32_t id = (0x1 << 31)
        | ((m_bus & 0xFF) << 16)
        | ((m_device & 0x1F) << 11)
        | ((m_function & 0x07) << 8)
        | (registeroffset & 0xFC);
    
    jlos_port32_bit_write(&self->m_command_port, id);
    uint32_t result = jlos_port32_bit_read(&self->m_data_port);
    return result >> (8 * (registeroffset % 4));
}

uint16_t jlos_pci_controller_read16(jlos_pci_controller_t* self, uint16_t m_bus, uint16_t m_device,
    uint16_t m_function, uint32_t registeroffset)
{
    return (uint16_t)jlos_pci_controller_read(self, m_bus, m_device, m_function, registeroffset) & 0xFFFF;
}

void jlos_pci_controller_write(jlos_pci_controller_t* self, uint16_t m_bus, uint16_t m_device,
    uint16_t m_function, uint32_t registeroffset, uint32_t value)
{
    uint32_t id = (0x1 << 31)
        | ((m_bus & 0xFF) << 16)
        | ((m_device & 0x1F) << 11)
        | ((m_function & 0x07) << 8)
        | (registeroffset & 0xFC);
    
    jlos_port32_bit_write(&self->m_command_port, id);
    jlos_port32_bit_write(&self->m_data_port, value);
}

void jlos_pci_controller_write16(jlos_pci_controller_t* self, uint16_t m_bus, uint16_t m_device,
    uint16_t m_function, uint32_t registeroffset, uint16_t value)
{
    uint32_t id = (0x1 << 31)
        | ((m_bus & 0xFF) << 16)
        | ((m_device & 0x1F) << 11)
        | ((m_function & 0x07) << 8)
        | (registeroffset & 0xFC);
    
    jlos_port32_bit_write(&self->m_command_port, id);
    jlos_port32_bit_write(&self->m_data_port, value);
}

bool jlos_pci_controller_device_has_functions(jlos_pci_controller_t* self, uint16_t m_bus, uint16_t m_device)
{
    return jlos_pci_controller_read(self, m_bus, m_device, 0, 0x0E) & (1 << 7);
}

void jlos_pci_controller_select_drivers(jlos_pci_controller_t* self, jlos_driver_manager_t *driver_manager,
    jlos_interrupt_manager_t *interrupts)
{
    for (int m_bus = 0; m_bus < 8; m_bus++) {
        for (int m_device = 0; m_device < 32; m_device++) {
            int num_functions = jlos_pci_controller_device_has_functions(self, m_bus, m_device) ? 8 : 1;
            for (int m_function = 0; m_function < num_functions; m_function++) {
                jlos_pci_device_descriptor_t dev =
                    jlos_pci_controller_get_device_descriptor(self, m_bus, m_device, m_function);

                if (dev.m_vendor_id == 0x0000 || dev.m_vendor_id == 0xFFFF) {
                    continue;
                }
                for (int bar_num = 0; bar_num < 6; bar_num++) {
                    jlos_pci_bar_t bar =
                        jlos_pci_controller_get_base_address_register(self, m_bus, m_device, m_function, bar_num);
                    if (bar.m_address && (bar.m_type == JLOS_PCI_INPUT_OUTPUT)) {
                        dev.m_port_base = (uint32_t)bar.m_address;
                    }
                }
                jlos_driver_t *driver = jlos_pci_controller_get_driver(self, dev, interrupts);
                if (driver != NULL) {
                    jlos_driver_manager_add_driver(driver_manager, driver);
                }
            }
        }
    }
}

jlos_pci_bar_t jlos_pci_controller_get_base_address_register(jlos_pci_controller_t* self, uint16_t m_bus, uint16_t m_device, uint16_t m_function, uint16_t bar)
{
    jlos_pci_bar_t result = {false, NULL, 0, JLOS_PCI_MEMORY_MAPPING};
    uint32_t headertype = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x0E) & 0x7F;
    int maxbars = 6 - (4 * headertype);
    if (bar >= maxbars) {
        return result;
    }
    uint32_t bar_value = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x10 + 4 * bar);
    result.m_type = (bar_value & 0x1) ? JLOS_PCI_INPUT_OUTPUT : JLOS_PCI_MEMORY_MAPPING;
    
    if (bar_value == 0) {
        return result;
    }
    
    jlos_pci_controller_write(self, m_bus, m_device, m_function, 0x10 + 4 * bar, 0xFFFFFFFF);
    uint32_t size_value = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x10 + 4 * bar);
    
    if (result.m_type == JLOS_PCI_INPUT_OUTPUT) {
        size_value &= ~0x3;
        uint32_t bar_size = (~size_value) + 1;
        result.m_size = bar_size;
        jlos_pci_controller_write(self, m_bus, m_device, m_function, 0x10 + 4 * bar, bar_value);
        result.m_address = (uint8_t *)(bar_value & ~0x3);
        result.m_prefetchable = false;
    } else {
        size_value &= ~0xF;
        uint32_t bar_size = (~size_value) + 1;
        result.m_size = bar_size;
        jlos_pci_controller_write(self, m_bus, m_device, m_function, 0x10 + 4 * bar, bar_value);
        result.m_address = (uint8_t *)(bar_value & ~0xF);
        result.m_prefetchable = (bar_value & 0x8) != 0;
    }
    return result;
}

jlos_driver_t *jlos_pci_network_controller_handle(jlos_pci_controller_t* self, jlos_pci_device_descriptor_t dev, jlos_interrupt_manager_t *interrupts)
{
    jlos_driver_t *driver = NULL;
    switch (dev.m_vendor_id) {
        case 0x1022: {
            uint16_t command = jlos_pci_controller_read16(self, dev.m_bus, dev.m_device, dev.m_function, 0x04);
            command |= 0x07;
            jlos_pci_controller_write16(self, dev.m_bus, dev.m_device, dev.m_function, 0x04, command);
            switch (dev.m_device_id) {
                case 0x2000: {
                    printf("AMD am79c973 PCI command: 0x");
                    printf_hex32(command);
                    printf("\n");

                    printf("Allocating AMD am79c973 driver structure...\n");
                    driver = (jlos_driver_t *)jlos_malloc(sizeof(jlos_amd_am79c973_t));
                    if (driver) {
                        printf("AMD am79c973 driver allocated at: 0x");
                        printf_hex32((uint32_t)driver);
                        printf("\n");
                        jlos_amd_am79c973_init((jlos_amd_am79c973_t*)driver, &dev, interrupts);
                        return driver;
                    }
                    printf("AMD am79c973 allocation FAILED\n");
                    break;
                }
            }
            break;
        }
        case 0x8086:
            break;
    }
    return driver;
}

jlos_driver_t *jlos_pci_controller_get_driver(jlos_pci_controller_t* self, jlos_pci_device_descriptor_t dev, jlos_interrupt_manager_t *interrupts)
{
    switch (dev.m_class_id) {
        case 0x02:
            return jlos_pci_network_controller_handle(self, dev, interrupts);
        default:
            break;
    }
    return NULL;
}

jlos_pci_device_descriptor_t jlos_pci_controller_get_device_descriptor(jlos_pci_controller_t* self, uint16_t m_bus, uint16_t m_device, uint16_t m_function)
{
    jlos_pci_device_descriptor_t result;
    result.m_bus = m_bus;
    result.m_device = m_device;
    result.m_function = m_function;

    result.m_vendor_id = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x00);
    result.m_device_id = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x02);

    result.m_class_id = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x0b);
    result.m_subclass_id = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x0a);
    result.m_interface_id = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x09);

    result.m_revision = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x08);
    result.m_interrupt = jlos_pci_controller_read(self, m_bus, m_device, m_function, 0x3c);

    return result; 
}