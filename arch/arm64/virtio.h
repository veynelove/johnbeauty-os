/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_VIRTIO_H
#define _JLOS_ARCH_ARM64_VIRTIO_H

#include <common/types.h>

#define JLOS_VIRTIO_MMIO_MAGIC_VALUE     0x74726976UL
#define JLOS_VIRTIO_MMIO_VERSION_MODERN  2

#define JLOS_VIRTIO_DEVICE_ID_NET        1
#define JLOS_VIRTIO_DEVICE_ID_BLK        2

#define JLOS_VIRTIO_REG_MAGIC            0x000
#define JLOS_VIRTIO_REG_VERSION          0x004
#define JLOS_VIRTIO_REG_DEVICE_ID        0x008
#define JLOS_VIRTIO_REG_DEV_FEATURES     0x010
#define JLOS_VIRTIO_REG_DEV_FEATURES_SEL 0x014
#define JLOS_VIRTIO_REG_DRV_FEATURES     0x020
#define JLOS_VIRTIO_REG_DRV_FEATURES_SEL 0x024
#define JLOS_VIRTIO_REG_QUEUE_SEL        0x030
#define JLOS_VIRTIO_REG_QUEUE_NUM_MAX    0x034
#define JLOS_VIRTIO_REG_QUEUE_NUM        0x038
#define JLOS_VIRTIO_REG_QUEUE_READY      0x044
#define JLOS_VIRTIO_REG_QUEUE_NOTIFY     0x050
#define JLOS_VIRTIO_REG_INTR_STATUS      0x060
#define JLOS_VIRTIO_REG_INTR_ACK         0x064
#define JLOS_VIRTIO_REG_DEVICE_STATUS    0x070
#define JLOS_VIRTIO_REG_QUEUE_DESC       0x080
#define JLOS_VIRTIO_REG_QUEUE_AVAIL      0x090
#define JLOS_VIRTIO_REG_QUEUE_USED       0x0a0
#define JLOS_VIRTIO_REG_CONFIG           0x100

#define JLOS_VIRTIO_STATUS_ACKNOWLEDGE   (1U << 0)
#define JLOS_VIRTIO_STATUS_DRIVER        (1U << 1)
#define JLOS_VIRTIO_STATUS_DRIVER_OK     (1U << 2)
#define JLOS_VIRTIO_STATUS_FEATURES_OK   (1U << 3)
#define JLOS_VIRTIO_STATUS_FAILED        (1U << 7)

#define JLOS_VIRTIO_FEATURES_SEL_DEVICE  0
#define JLOS_VIRTIO_FEATURES_SEL_VERSION 1
#define JLOS_VIRTIO_F_VERSION_1_BIT      0

#define JLOS_VIRTQ_DESC_F_NEXT           (1U << 0)
#define JLOS_VIRTQ_DESC_F_WRITE          (1U << 1)
#define JLOS_VIRTQ_AVAIL_F_NO_INTERRUPT  (1U << 0)

#define JLOS_VIRTQ_MAX_SIZE              256

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} jlos_virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[JLOS_VIRTQ_MAX_SIZE];
} jlos_virtq_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} jlos_virtq_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    jlos_virtq_used_elem_t ring[JLOS_VIRTQ_MAX_SIZE];
} jlos_virtq_used_t;

#endif
