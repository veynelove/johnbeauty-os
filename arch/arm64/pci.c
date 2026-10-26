/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/pci.h>

void jlos_hal_pci_init(void)
{
}

uint32_t jlos_hal_pci_config_read32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg)
{
    (void)self; (void)bus; (void)dev; (void)func; (void)reg;
    return 0xFFFFFFFFu;
}

void jlos_hal_pci_config_write32(jlos_hal_pci_controller_t *self, uint16_t bus, uint16_t dev, uint16_t func, uint16_t reg, uint32_t val)
{
    (void)self; (void)bus; (void)dev; (void)func; (void)reg; (void)val;
}

void jlos_hal_pci_enumerate_and_bind_drivers(void)
{
}
