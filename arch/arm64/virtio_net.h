/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_ARCH_ARM64_VIRTIO_NET_H
#define _JLOS_ARCH_ARM64_VIRTIO_NET_H

#include <common/types.h>
#include <arch/arm64/virtio.h>
#include <hal/irq.h>
#include <hal/net.h>

#define JLOS_VIRTIO_NET_CFG_MAC          0x000
#define JLOS_VIRTIO_NET_HDR_LEN          12
#define JLOS_VIRTIO_NET_RX_QUEUE         0
#define JLOS_VIRTIO_NET_TX_QUEUE         1
#define JLOS_VIRTIO_NET_QUEUE_SIZE       64
#define JLOS_VIRTIO_NET_TX_BUFFERS       8
#define JLOS_VIRTIO_NET_BUF_SIZE         2048
#define JLOS_VIRTIO_NET_SPI_BASE         32

typedef struct {
    uintptr_t               base;
    uint32_t                intid;
    jlos_hal_net_dev_t      *dev;
    jlos_virtq_desc_t       *rx_desc;
    jlos_virtq_avail_t      *rx_avail;
    jlos_virtq_used_t       *rx_used;
    jlos_virtq_desc_t       *tx_desc;
    jlos_virtq_avail_t      *tx_avail;
    jlos_virtq_used_t       *tx_used;
    uint8_t                 *rx_buffers[JLOS_VIRTIO_NET_QUEUE_SIZE];
    uint8_t                 *tx_buffers[JLOS_VIRTIO_NET_TX_BUFFERS];
    uint64_t                tx_buffer_phys[JLOS_VIRTIO_NET_TX_BUFFERS];
    bool                    tx_in_use[JLOS_VIRTIO_NET_TX_BUFFERS];
    uint32_t                qsize;
    uint16_t                rx_last_used;
    uint16_t                tx_last_used;
    jlos_irq_handler_t      irq;
} jlos_virtio_net_ctx_t;

#endif
