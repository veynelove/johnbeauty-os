#include <hal/pci.h>
#include <hal/diag.h>

#if KERNEL_CONFIG_HARDWARE_ARCH == KERNEL_CONFIG_ARCH_X86

/* ---------------- 生命周期 ---------------- */
void jlos_hal_pci_init(jlos_hal_pci_controller_t *self)
{ jlos_pci_controller_init(self); }

/* ---------------- Config 空间读 ----------------
 * PCI Config Mechanism #1 只能 32-bit 对齐读写（低 2 bits == 0）。
 * 8/16 位读我们自己处理：读对应 DWORD 再按 offset&3 shift/mask。 */
uint32_t jlos_hal_pci_config_read32(jlos_hal_pci_controller_t *self,
                                    uint16_t bus, uint16_t dev, uint16_t func,
                                    uint16_t reg)
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

uint16_t jlos_hal_pci_config_read16(jlos_hal_pci_controller_t *self,
                                    uint16_t bus, uint16_t dev, uint16_t func,
                                    uint16_t reg)
{
    uint32_t dword = jlos_hal_pci_config_read32(self, bus, dev, func, reg);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    return (uint16_t)((dword >> shift) & 0xFFFFu);
}

uint8_t jlos_hal_pci_config_read8(jlos_hal_pci_controller_t *self,
                                  uint16_t bus, uint16_t dev, uint16_t func,
                                  uint16_t reg)
{
    uint32_t dword = jlos_hal_pci_config_read32(self, bus, dev, func, reg);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    return (uint8_t)((dword >> shift) & 0xFFu);
}

/* ---------------- Config 空间写 ----------------
 * 8/16 位写：读-改-写回对应 DWORD。性能不是最优但正确性优先。 */
void jlos_hal_pci_config_write32(jlos_hal_pci_controller_t *self,
                                 uint16_t bus, uint16_t dev, uint16_t func,
                                 uint16_t reg, uint32_t val)
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

void jlos_hal_pci_config_write16(jlos_hal_pci_controller_t *self,
                                 uint16_t bus, uint16_t dev, uint16_t func,
                                 uint16_t reg, uint16_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t dword = jlos_pci_controller_read(self, bus, dev, func, aligned);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    dword &= ~((uint32_t)0xFFFFu << shift);
    dword |=  ((uint32_t)val << shift);
    jlos_pci_controller_write(self, bus, dev, func, aligned, dword);
}

void jlos_hal_pci_config_write8(jlos_hal_pci_controller_t *self,
                                uint16_t bus, uint16_t dev, uint16_t func,
                                uint16_t reg, uint8_t val)
{
    uint16_t aligned = reg & (uint16_t)~3u;
    uint32_t dword = jlos_pci_controller_read(self, bus, dev, func, aligned);
    unsigned shift = (unsigned)(reg & 3) * 8u;
    dword &= ~((uint32_t)0xFFu << shift);
    dword |=  ((uint32_t)val << shift);
    jlos_pci_controller_write(self, bus, dev, func, aligned, dword);
}

/* ---------------- 枚举 + 驱动绑定 ---------------- */
void jlos_hal_pci_enumerate_and_bind_drivers(jlos_hal_pci_controller_t *self,
                                             jlos_hal_pci_drv_mgr_t *drv_mgr,
                                             jlos_hal_pci_irq_mgr_t *irq_mgr)
{
    /* 0xCF8-0xCFF 已在 hal_arch_init() 时注册成 "PCI Config Mechanism #1"，直接用 */
    jlos_pci_controller_select_drivers(self,
                                       (jlos_driver_manager_t *)drv_mgr,
                                       (jlos_interrupt_manager_t *)irq_mgr);
}

/* ---------------- 设备/BAR 查询 ---------------- */
jlos_hal_pci_device_t jlos_hal_pci_get_device_descriptor(jlos_hal_pci_controller_t *self,
                                                         uint16_t bus, uint16_t dev, uint16_t func)
{ return jlos_pci_controller_get_device_descriptor(self, bus, dev, func); }

jlos_hal_pci_bar_t jlos_hal_pci_get_bar(jlos_hal_pci_controller_t *self,
                                        uint16_t bus, uint16_t dev, uint16_t func, uint16_t bar_index)
{ return jlos_pci_controller_get_base_address_register(self, bus, dev, func, bar_index); }

#endif
