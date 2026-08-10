#ifndef __JLOS_HAL_DEVICE_H
#define __JLOS_HAL_DEVICE_H

#include <tools/config.h>
#include <common/types.h>
#include <drivers/driver.h>

#define JLOS_HAL_MAX_DEVICES    32
#define JLOS_HAL_MAX_RESOURCES  8

typedef enum {
    JLOS_DEV_BUS_NONE     = 0,
    JLOS_DEV_BUS_PCI      = 1,
    JLOS_DEV_BUS_PLATFORM = 2,
    JLOS_DEV_BUS_AMBA     = 3,
} jlos_dev_bus_t;

typedef enum {
    JLOS_RES_NONE     = 0,
    JLOS_RES_IO_PORT  = 1,
    JLOS_RES_MMIO     = 2,
    JLOS_RES_IRQ      = 3,
    JLOS_RES_DMA_CHAN  = 4,
} jlos_resource_type_t;

typedef struct {
    jlos_resource_type_t type;
    uint32_t start;
    uint32_t end;
    uint32_t flags;
} jlos_resource_t;

typedef struct jlos_device {
    const char        *name;
    jlos_dev_bus_t    bus_type;
    bool              registered;

    union {
        struct {
            uint8_t  bus;
            uint8_t  dev;
            uint8_t  func;
            uint32_t bar[6];
        } pci;
        struct {
            uint32_t mmio_base;
            uint32_t mmio_size;
            uint8_t  irq;
        } platform;
    } businfo;

    jlos_resource_t   resources[JLOS_HAL_MAX_RESOURCES];
    jlos_driver_t *drv;
} jlos_device_t;

int  jlos_hal_device_register(jlos_device_t *dev);

#endif
