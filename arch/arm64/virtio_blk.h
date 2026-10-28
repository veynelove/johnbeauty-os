/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0.
 */

#ifndef _JLOS_ARCH_ARM64_VIRTIO_BLK_H
#define _JLOS_ARCH_ARM64_VIRTIO_BLK_H

#include <common/types.h>
#include <arch/arm64/virtio.h>

#define JLOS_VIRTIO_BLK_CFG_CAPACITY     0x000

#define JLOS_VIRTIO_BLK_T_IN             0
#define JLOS_VIRTIO_BLK_T_OUT            1
#define JLOS_VIRTIO_BLK_T_FLUSH          4
#define JLOS_VIRTIO_BLK_F_FLUSH_BIT      9

#define JLOS_VIRTIO_BLK_S_OK             0

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} jlos_virtio_blk_req_t;

typedef struct {
    jlos_virtio_blk_req_t   req;
    uint8_t                 status;
} jlos_virtio_blk_dma_t;

typedef struct {
    uintptr_t              base;
    jlos_virtq_desc_t      *desc;
    jlos_virtq_avail_t     *avail;
    jlos_virtq_used_t      *used;
    jlos_virtio_blk_dma_t  *dma;
    uint64_t               dma_phys;
    uint32_t               qsize;
    uint16_t               last_used;
    bool                   flush_ok;
} jlos_virtio_blk_ctx_t;

#endif
