/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <arch/arm64/virtio_blk.h>
#include <common/hardware/fdt.h>
#include <common/types.h>
#include <hal/barrier.h>
#include <hal/block.h>
#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <hal/io.h>
#include <hal/paging.h>
#include <kernel/initcall.h>
#include <kernel/page_frame_allocator.h>

#define JLOS_KERNEL_LOG_SUBSYS "virtio"
#include <kernel/printk.h>

static jlos_hal_block_dev_t s_virtio_blk_dev;
static jlos_virtio_blk_ctx_t s_virtio_blk_ctx;

static uint32_t virtio_reg_read(jlos_virtio_blk_ctx_t *ctx, uint32_t off)
{
    return jlos_io_mmio_read32((volatile void *)(ctx->base + off));
}

static void virtio_reg_write(jlos_virtio_blk_ctx_t *ctx, uint32_t off, uint32_t val)
{
    jlos_io_mmio_write32((volatile void *)(ctx->base + off), val);
}

static void virtio_reg_write64(jlos_virtio_blk_ctx_t *ctx, uint32_t off, uint64_t val)
{
    virtio_reg_write(ctx, off, (uint32_t)val);
    virtio_reg_write(ctx, off + 4, (uint32_t)(val >> 32));
}

static void virtio_status_add(jlos_virtio_blk_ctx_t *ctx, uint32_t bits)
{
    uint32_t status = virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS, status | bits);
}

static bool fdt_node_enabled(int32_t node)
{
    const char *status = (const char *)jlos_fdt_get_prop(node, "status", nullptr);
    return !status || jlos_strcmp(status, "okay") == 0;
}

static int32_t virtio_blk_probe_fdt(jlos_virtio_blk_ctx_t *ctx)
{
    int32_t root = jlos_fdt_find_node("/");
    if (root < 0) {
        return -1;
    }
    for (int32_t node = jlos_fdt_first_subnode(root); node >= 0; node = jlos_fdt_next_subnode(node)) {
        const char *name = jlos_fdt_get_node_name(node);
        if (!name || jlos_strncmp(name, "virtio_mmio", 11) != 0) {
            continue;
        }
        if (!fdt_node_enabled(node)) {
            continue;
        }
        uint32_t len = 0;
        const uint32_t *reg = (const uint32_t *)jlos_fdt_get_prop(node, "reg", &len);
        if (!reg || len < 4 * JLOS_FDT_TOKEN_SIZE) {
            continue;
        }
        uintptr_t base = (uintptr_t)jlos_fdt_read_u64(reg) + KERNEL_DIRECT_MAP_BASE;
        uint32_t magic = jlos_io_mmio_read32((volatile void *)(base + JLOS_VIRTIO_REG_MAGIC));
        uint32_t version = jlos_io_mmio_read32((volatile void *)(base + JLOS_VIRTIO_REG_VERSION));
        uint32_t devid = jlos_io_mmio_read32((volatile void *)(base + JLOS_VIRTIO_REG_DEVICE_ID));

        if (magic != JLOS_VIRTIO_MMIO_MAGIC_VALUE || version != JLOS_VIRTIO_MMIO_VERSION_MODERN) {
            continue;
        }
        if (devid != JLOS_VIRTIO_DEVICE_ID_BLK) {
            continue;
        }
        ctx->base = base;
        return 0;
    }
    return -1;
}

static uint64_t virtio_blk_read_capacity(jlos_virtio_blk_ctx_t *ctx)
{
    uint32_t lo = virtio_reg_read(ctx, JLOS_VIRTIO_REG_CONFIG + JLOS_VIRTIO_BLK_CFG_CAPACITY);
    uint32_t hi = virtio_reg_read(ctx, JLOS_VIRTIO_REG_CONFIG + JLOS_VIRTIO_BLK_CFG_CAPACITY + 4);
    return lo | ((uint64_t)hi << 32);
}

static int32_t virtio_blk_init_hw(jlos_virtio_blk_ctx_t *ctx)
{
    if (virtio_blk_probe_fdt(ctx) < 0) {
        printk_err("no virtio-mmio blk device in fdt\n");
        return -1;
    }
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS, 0);
    virtio_status_add(ctx, JLOS_VIRTIO_STATUS_ACKNOWLEDGE | JLOS_VIRTIO_STATUS_DRIVER);

    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEV_FEATURES_SEL, JLOS_VIRTIO_FEATURES_SEL_DEVICE);
    uint32_t dev_features = virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEV_FEATURES);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEV_FEATURES_SEL, JLOS_VIRTIO_FEATURES_SEL_VERSION);
    uint32_t version_features = virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEV_FEATURES);


    if (!(version_features & (1U << JLOS_VIRTIO_F_VERSION_1_BIT))) {
        printk_err("device does not offer virtio version 1\n");
        return -1;
    }
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES_SEL, JLOS_VIRTIO_FEATURES_SEL_VERSION);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES, 1U << JLOS_VIRTIO_F_VERSION_1_BIT);

    ctx->flush_ok = (dev_features & (1U << JLOS_VIRTIO_BLK_F_FLUSH_BIT)) != 0;
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES_SEL, JLOS_VIRTIO_FEATURES_SEL_DEVICE);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES, ctx->flush_ok ? (1U << JLOS_VIRTIO_BLK_F_FLUSH_BIT) : 0);

    virtio_status_add(ctx, JLOS_VIRTIO_STATUS_FEATURES_OK);
    if (!(virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS) & JLOS_VIRTIO_STATUS_FEATURES_OK)) {
        printk_err("features negotiate failed\n");
        return -1;
    }

    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_SEL, 0);
    uint32_t qmax = virtio_reg_read(ctx, JLOS_VIRTIO_REG_QUEUE_NUM_MAX);
    if (qmax == 0) {
        printk_err("queue 0 unavailable\n");
        return -1;
    }
    uint32_t qsize = qmax < JLOS_VIRTQ_MAX_SIZE ? qmax : JLOS_VIRTQ_MAX_SIZE;
    if ((qsize & (qsize - 1)) != 0) {
        printk_err("unsupported queue size %u\n", qsize);
        return -1;
    }
    ctx->desc = (jlos_virtq_desc_t *)jlos_page_frame_malloc();
    ctx->avail = (jlos_virtq_avail_t *)jlos_page_frame_malloc();
    ctx->used = (jlos_virtq_used_t *)jlos_page_frame_malloc();
    ctx->dma = (jlos_virtio_blk_dma_t *)jlos_page_frame_malloc();
    if (!ctx->desc || !ctx->avail || !ctx->used || !ctx->dma) {
        printk_err("virtqueue alloc failed\n");
        return -1;
    }
    jlos_memset(ctx->desc, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->avail, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->used, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->dma, 0, JLOS_PAGE_FRAME_SIZE);
    ctx->avail->flags = JLOS_VIRTQ_AVAIL_F_NO_INTERRUPT;
    ctx->dma_phys = VIRT_TO_PHYS(ctx->dma);
    ctx->qsize = qsize;
    ctx->last_used = 0;

    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_NUM, qsize);
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_DESC, VIRT_TO_PHYS(ctx->desc));
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_AVAIL, VIRT_TO_PHYS(ctx->avail));
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_USED, VIRT_TO_PHYS(ctx->used));
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_READY, 1);

    virtio_status_add(ctx, JLOS_VIRTIO_STATUS_DRIVER_OK);
    if (virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS) & JLOS_VIRTIO_STATUS_FAILED) {
        printk_err("device entered failed state\n");
        return -1;
    }
    return 0;
}

static int virtio_blk_transfer(jlos_virtio_blk_ctx_t *ctx, uint32_t type,
    uint64_t sector, void *buf, uint32_t bytes)
{
    uint32_t ndesc = 2;
    if (buf && bytes) {
        uint32_t span = (uint32_t)(((uintptr_t)buf & (JLOS_PAGE_SIZE - 1)) + bytes);
        ndesc += (uint32_t)(JLOS_PAGE_ALIGN_UP(span) >> JLOS_PAGE_SHIFT);
    }
    if (ndesc > ctx->qsize) {
        return -1;
    }

    ctx->dma->req.type = type;
    ctx->dma->req.sector = sector;
    ctx->dma->status = 0xFF;
    ctx->desc[0].addr = ctx->dma_phys;
    ctx->desc[0].len = sizeof(jlos_virtio_blk_req_t);
    ctx->desc[0].flags = JLOS_VIRTQ_DESC_F_NEXT;
    ctx->desc[0].next = 1;

    uint32_t di = 1;
    uintptr_t va = (uintptr_t)buf;
    uint32_t left = bytes;
    while (left > 0) {
        uint32_t chunk = (uint32_t)(JLOS_PAGE_SIZE - (va & (JLOS_PAGE_SIZE - 1)));
        if (chunk > left) {
            chunk = left;
        }
        uintptr_t pa = jlos_arch_get_phys_entry(&s_kernel_paging_context, va);
        if (pa == 0) {
            return -1;
        }
        ctx->desc[di].addr = pa;
        ctx->desc[di].len = chunk;
        ctx->desc[di].flags = JLOS_VIRTQ_DESC_F_NEXT |
            (type == JLOS_VIRTIO_BLK_T_IN ? JLOS_VIRTQ_DESC_F_WRITE : 0);
        ctx->desc[di].next = di + 1;
        di++;
        va += chunk;
        left -= chunk;
    }

    ctx->desc[di].addr = ctx->dma_phys + offsetof(jlos_virtio_blk_dma_t, status);
    ctx->desc[di].len = 1;
    ctx->desc[di].flags = JLOS_VIRTQ_DESC_F_WRITE;
    ctx->desc[di].next = 0;

    ctx->avail->ring[ctx->avail->idx & (ctx->qsize - 1)] = 0;
    ctx->avail->idx++;

    jlos_wmb();
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_NOTIFY, 0);
    while (ctx->last_used == *(volatile uint16_t *)&ctx->used->idx) {
        jlos_hal_cpu_relax();
    }
    ctx->last_used++;

    return ctx->dma->status == JLOS_VIRTIO_BLK_S_OK ? 0 : -1;
}

static jlos_virtio_blk_ctx_t *virtio_blk_ctx(jlos_hal_block_dev_t *self)
{
    return *(jlos_virtio_blk_ctx_t **)(void *)self->priv;
}

static void virtio_blk_dev_init(jlos_hal_block_dev_t *self)
{
    jlos_virtio_blk_ctx_t *ctx = *(jlos_virtio_blk_ctx_t **)(void *)self->priv;
    if (virtio_blk_init_hw(ctx) < 0) {
        return;
    }
    self->bytes_per_sector = JLOS_BLOCK_SECTOR_SIZE;
    self->total_sectors = virtio_blk_read_capacity(ctx);
    self->inited = 1;
}

static void virtio_blk_dev_destroy(jlos_hal_block_dev_t *self)
{
    jlos_virtio_blk_ctx_t *ctx = virtio_blk_ctx(self);
    jlos_page_frame_free(ctx->desc);
    jlos_page_frame_free(ctx->avail);
    jlos_page_frame_free(ctx->used);
    jlos_page_frame_free(ctx->dma);
    ctx->desc = nullptr;
    ctx->avail = nullptr;
    ctx->used = nullptr;
    ctx->dma = nullptr;
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS, 0);
    self->inited = 0;
}

static void virtio_blk_dev_flush(jlos_hal_block_dev_t *self)
{
    if (!self->inited || !virtio_blk_ctx(self)->flush_ok) {
        return;
    }
    virtio_blk_transfer(virtio_blk_ctx(self), JLOS_VIRTIO_BLK_T_FLUSH, 0, nullptr, 0);
}

static int virtio_blk_dev_read_sectors(jlos_hal_block_dev_t *self, uint64_t lba, uint8_t *buf, uint32_t count)
{
    if (!self->inited || (lba + count) > self->total_sectors) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    return virtio_blk_transfer(virtio_blk_ctx(self), JLOS_VIRTIO_BLK_T_IN, lba, buf,
        count * self->bytes_per_sector);
}

static int virtio_blk_dev_write_sectors(jlos_hal_block_dev_t *self, uint64_t lba, const uint8_t *buf, uint32_t count)
{
    if (!self->inited || (lba + count) > self->total_sectors) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    return virtio_blk_transfer(virtio_blk_ctx(self), JLOS_VIRTIO_BLK_T_OUT, lba, (void *)buf,
        count * self->bytes_per_sector);
}

static const jlos_hal_block_ops_t s_virtio_blk_ops = {
    .init           = virtio_blk_dev_init,
    .destroy        = virtio_blk_dev_destroy,
    .identify       = nullptr,
    .flush          = virtio_blk_dev_flush,
    .read_sectors   = virtio_blk_dev_read_sectors,
    .write_sectors  = virtio_blk_dev_write_sectors,
};

static void jlos_virtio_blk_dev_init(void)
{
    static_assert(sizeof(jlos_virtio_blk_ctx_t *) <= JLOS_HAL_BLOCK_PRIV_SIZE, "block priv too small for virtio blk");
    s_virtio_blk_dev.dev_type = JLOS_HAL_BLOCK_DEV_VIRTIO_BLK;
    s_virtio_blk_dev.ops = &s_virtio_blk_ops;
    *(jlos_virtio_blk_ctx_t **)(void *)s_virtio_blk_dev.priv = &s_virtio_blk_ctx;
    virtio_blk_dev_init(&s_virtio_blk_dev);
    if (s_virtio_blk_dev.inited) {
        jlos_hal_block_set_primary_dev(&s_virtio_blk_dev);
        printk_info("blk0 ready, sectors = %lu\n", (unsigned long)s_virtio_blk_dev.total_sectors);
    } else {
        printk_err("virtio blk init failed\n");
    }
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, jlos_virtio_blk_dev_init);
