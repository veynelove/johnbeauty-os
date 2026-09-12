#ifndef _JLOS_HAL_PCI_H
#define _JLOS_HAL_PCI_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/irq.h>
#include <drivers/driver.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86
#include <arch/x86/pci.h>
#else
#error "[HAL PCI] Unsupported arch"
#endif

/* 命名别名：把 arch 层的名字映射到 HAL 层命名空间 */
typedef jlos_pci_controller_t         jlos_hal_pci_controller_t;
typedef jlos_pci_device_descriptor_t  jlos_hal_pci_device_t;
typedef jlos_pci_bar_t                jlos_hal_pci_bar_t;
typedef jlos_irq_manager_t            jlos_hal_pci_irq_mgr_t;
typedef jlos_driver_manager_t         jlos_hal_pci_drv_mgr_t;

/* ---------------- 控制器生命周期 ---------------- */
void jlos_hal_pci_init(void);

/* ---------------- Config 空间读写（任意宽度） ---------------- */
uint32_t jlos_hal_pci_config_read32(jlos_hal_pci_controller_t *self,
                                    uint16_t bus, uint16_t dev, uint16_t func,
                                    uint16_t reg);
uint16_t jlos_hal_pci_config_read16(jlos_hal_pci_controller_t *self,
                                    uint16_t bus, uint16_t dev, uint16_t func,
                                    uint16_t reg);
uint8_t  jlos_hal_pci_config_read8 (jlos_hal_pci_controller_t *self,
                                    uint16_t bus, uint16_t dev, uint16_t func,
                                    uint16_t reg);
void jlos_hal_pci_config_write32(jlos_hal_pci_controller_t *self,
                                 uint16_t bus, uint16_t dev, uint16_t func,
                                 uint16_t reg, uint32_t val);
void jlos_hal_pci_config_write16(jlos_hal_pci_controller_t *self,
                                 uint16_t bus, uint16_t dev, uint16_t func,
                                 uint16_t reg, uint16_t val);
void jlos_hal_pci_config_write8 (jlos_hal_pci_controller_t *self,
                                 uint16_t bus, uint16_t dev, uint16_t func,
                                 uint16_t reg, uint8_t val);

/* ---------------- 枚举与驱动绑定 ---------------- */
/* 遍历总线上所有设备（0-255 bus，0-31 dev，0-7 func），把能认出的设备绑到 driver manager。
 * 底层包装 arch 的 jlos_pci_controller_select_drivers */
void jlos_hal_pci_enumerate_and_bind_drivers(void);

/* ---------------- 设备描述符 / BAR 读取 ---------------- */
jlos_hal_pci_device_t jlos_hal_pci_get_device_descriptor(jlos_hal_pci_controller_t *self,
                                                         uint16_t bus, uint16_t dev, uint16_t func);
jlos_hal_pci_bar_t jlos_hal_pci_get_bar(jlos_hal_pci_controller_t *self,
                                        uint16_t bus, uint16_t dev, uint16_t func, uint16_t bar_index);

#endif
