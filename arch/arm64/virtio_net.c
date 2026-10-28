/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm64/virtio_net.h>
#include <arch/arm64/gic.h>
#include <common/hardware/fdt.h>
#include <common/types.h>
#include <hal/barrier.h>
#include <hal/hal_arch.h>
#include <hal/io.h>
#include <hal/paging.h>
#include <kernel/initcall.h>
#include <kernel/page_frame_allocator.h>

#define JLOS_KERNEL_LOG_SUBSYS "virtio"
#include <kernel/printk.h>

static jlos_hal_net_dev_t s_virtio_net_dev;
static jlos_virtio_net_ctx_t s_virtio_net_ctx;

static uint32_t virtio_reg_read(jlos_virtio_net_ctx_t *ctx, uint32_t off)
{
    return jlos_io_mmio_read32((volatile void *)(ctx->base + off));
}

static void virtio_reg_write(jlos_virtio_net_ctx_t *ctx, uint32_t off, uint32_t val)
{
    jlos_io_mmio_write32((volatile void *)(ctx->base + off), val);
}

static void virtio_reg_write64(jlos_virtio_net_ctx_t *ctx, uint32_t off, uint64_t val)
{
    virtio_reg_write(ctx, off, (uint32_t)val);
    virtio_reg_write(ctx, off + 4, (uint32_t)(val >> 32));
}

static void virtio_status_add(jlos_virtio_net_ctx_t *ctx, uint32_t bits)
{
    uint32_t status = virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS, status | bits);
}

static jlos_virtio_net_ctx_t *virtio_net_ctx(jlos_hal_net_dev_t *self)
{
    return *(jlos_virtio_net_ctx_t **)(void *)self->priv;
}

static bool fdt_node_enabled(int32_t node)
{
    const char *status = (const char *)jlos_fdt_get_prop(node, "status", nullptr);
    return !status || jlos_strcmp(status, "okay") == 0;
}

static int32_t virtio_net_probe_fdt(jlos_virtio_net_ctx_t *ctx)
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
        if (devid != JLOS_VIRTIO_DEVICE_ID_NET) {
            continue;
        }
        const uint32_t *intr = (const uint32_t *)jlos_fdt_get_prop(node, "interrupts", &len);
        if (!intr || len < 2 * JLOS_FDT_TOKEN_SIZE) {
            continue;
        }
        ctx->base = base;
        ctx->intid = JLOS_VIRTIO_NET_SPI_BASE + jlos_fdt_read_u32(&intr[1]);
        return 0;
    }
    return -1;
}

static void virtio_net_rx_post(jlos_virtio_net_ctx_t *ctx, uint16_t id)
{
    ctx->rx_avail->ring[ctx->rx_avail->idx & (ctx->qsize - 1)] = id;
    jlos_wmb();
    ctx->rx_avail->idx++;
}

static int32_t virtio_net_setup_rx_queue(jlos_virtio_net_ctx_t *ctx)
{
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_SEL, JLOS_VIRTIO_NET_RX_QUEUE);
    uint32_t qmax = virtio_reg_read(ctx, JLOS_VIRTIO_REG_QUEUE_NUM_MAX);
    if (qmax == 0) {
        return -1;
    }
    uint32_t qsize = qmax < JLOS_VIRTIO_NET_QUEUE_SIZE ? qmax : JLOS_VIRTIO_NET_QUEUE_SIZE;
    if ((qsize & (qsize - 1)) != 0) {
        return -1;
    }
    ctx->qsize = qsize;
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_NUM, qsize);
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_DESC, VIRT_TO_PHYS(ctx->rx_desc));
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_AVAIL, VIRT_TO_PHYS(ctx->rx_avail));
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_USED, VIRT_TO_PHYS(ctx->rx_used));
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_READY, 1);
    return 0;
}

static int32_t virtio_net_setup_tx_queue(jlos_virtio_net_ctx_t *ctx)
{
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_SEL, JLOS_VIRTIO_NET_TX_QUEUE);
    uint32_t qmax = virtio_reg_read(ctx, JLOS_VIRTIO_REG_QUEUE_NUM_MAX);
    if (qmax == 0) {
        return -1;
    }
    uint32_t qsize = qmax < JLOS_VIRTIO_NET_QUEUE_SIZE ? qmax : JLOS_VIRTIO_NET_QUEUE_SIZE;
    if ((qsize & (qsize - 1)) != 0) {
        return -1;
    }
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_NUM, qsize);
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_DESC, VIRT_TO_PHYS(ctx->tx_desc));
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_AVAIL, VIRT_TO_PHYS(ctx->tx_avail));
    virtio_reg_write64(ctx, JLOS_VIRTIO_REG_QUEUE_USED, VIRT_TO_PHYS(ctx->tx_used));
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_READY, 1);
    return 0;
}

static int32_t virtio_net_alloc_queues(jlos_virtio_net_ctx_t *ctx)
{
    ctx->rx_desc = (jlos_virtq_desc_t *)jlos_page_frame_malloc();
    ctx->rx_avail = (jlos_virtq_avail_t *)jlos_page_frame_malloc();
    ctx->rx_used = (jlos_virtq_used_t *)jlos_page_frame_malloc();
    ctx->tx_desc = (jlos_virtq_desc_t *)jlos_page_frame_malloc();
    ctx->tx_avail = (jlos_virtq_avail_t *)jlos_page_frame_malloc();
    ctx->tx_used = (jlos_virtq_used_t *)jlos_page_frame_malloc();
    if (!ctx->rx_desc || !ctx->rx_avail || !ctx->rx_used ||
        !ctx->tx_desc || !ctx->tx_avail || !ctx->tx_used) {
        return -1;
    }
    jlos_memset(ctx->rx_desc, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->rx_avail, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->rx_used, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->tx_desc, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->tx_avail, 0, JLOS_PAGE_FRAME_SIZE);
    jlos_memset(ctx->tx_used, 0, JLOS_PAGE_FRAME_SIZE);
    for (uint32_t i = 0; i < JLOS_VIRTIO_NET_TX_BUFFERS; i++) {
        uint8_t *txb = (uint8_t *)jlos_page_frame_malloc();
        if (!txb) {
            return -1;
        }
        ctx->tx_buffers[i] = txb;
        ctx->tx_buffer_phys[i] = VIRT_TO_PHYS(txb);
        ctx->tx_in_use[i] = false;
        ctx->tx_desc[i].addr = ctx->tx_buffer_phys[i];
        ctx->tx_desc[i].len = JLOS_VIRTIO_NET_BUF_SIZE;
        ctx->tx_desc[i].flags = 0;
        ctx->tx_desc[i].next = 0;
    }
    return 0;
}

static int32_t virtio_net_rx_fill(jlos_virtio_net_ctx_t *ctx)
{
    for (uint16_t i = 0; i < ctx->qsize; i++) {
        uint8_t *buf = (uint8_t *)jlos_page_frame_malloc();
        if (!buf) {
            return -1;
        }
        ctx->rx_buffers[i] = buf;
        ctx->rx_desc[i].addr = VIRT_TO_PHYS(buf);
        ctx->rx_desc[i].len = JLOS_VIRTIO_NET_BUF_SIZE;
        ctx->rx_desc[i].flags = JLOS_VIRTQ_DESC_F_WRITE;
        ctx->rx_desc[i].next = 0;
        virtio_net_rx_post(ctx, i);
    }
    return 0;
}

static void virtio_net_rx_poll(jlos_virtio_net_ctx_t *ctx)
{
    volatile jlos_virtq_used_t *used = ctx->rx_used;
    while (ctx->rx_last_used != used->idx) {
        volatile jlos_virtq_used_elem_t *elem = &used->ring[ctx->rx_last_used & (ctx->qsize - 1)];
        uint16_t id = (uint16_t)elem->id;
        uint32_t len = elem->len;
        if (id >= ctx->qsize) {
            break;
        }
        uint8_t *buffer = ctx->rx_buffers[id];
        if (buffer && ctx->dev->on_receive && len > JLOS_VIRTIO_NET_HDR_LEN) {
            ctx->dev->on_receive(ctx->dev->receive_context, buffer + JLOS_VIRTIO_NET_HDR_LEN, len - JLOS_VIRTIO_NET_HDR_LEN);
        }
        virtio_net_rx_post(ctx, id);
        ctx->rx_last_used++;
    }
    jlos_wmb();
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_NOTIFY, JLOS_VIRTIO_NET_RX_QUEUE);
}

static void virtio_net_tx_reclaim(jlos_virtio_net_ctx_t *ctx)
{
    volatile jlos_virtq_used_t *used = ctx->tx_used;
    while (ctx->tx_last_used != used->idx) {
        volatile jlos_virtq_used_elem_t *elem = &used->ring[ctx->tx_last_used & (ctx->qsize - 1)];
        uint16_t id = (uint16_t)elem->id;
        if (id < JLOS_VIRTIO_NET_TX_BUFFERS) {
            ctx->tx_in_use[id] = false;
        }
        ctx->tx_last_used++;
    }
}

static uintptr_t virtio_net_handle_irq(jlos_irq_handler_t *handler, uintptr_t esp)
{
    jlos_virtio_net_ctx_t *ctx = container_of(handler, jlos_virtio_net_ctx_t, irq);
    uint32_t status = virtio_reg_read(ctx, JLOS_VIRTIO_REG_INTR_STATUS);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_INTR_ACK, status);
    virtio_net_tx_reclaim(ctx);
    virtio_net_rx_poll(ctx);
    return esp;
}

static int32_t virtio_net_init_hw(jlos_virtio_net_ctx_t *ctx, jlos_hal_net_dev_t *dev)
{
    if (virtio_net_probe_fdt(ctx) < 0) {
        printk_err("no virtio-mmio net device in fdt\n");
        return -1;
    }
    ctx->dev = dev;
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS, 0);
    virtio_status_add(ctx, JLOS_VIRTIO_STATUS_ACKNOWLEDGE | JLOS_VIRTIO_STATUS_DRIVER);

    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEV_FEATURES_SEL, JLOS_VIRTIO_FEATURES_SEL_VERSION);
    uint32_t version_features = virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEV_FEATURES);
    if (!(version_features & (1U << JLOS_VIRTIO_F_VERSION_1_BIT))) {
        printk_err("net device does not offer virtio version 1\n");
        return -1;
    }
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES_SEL, JLOS_VIRTIO_FEATURES_SEL_VERSION);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES, 1U << JLOS_VIRTIO_F_VERSION_1_BIT);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES_SEL, JLOS_VIRTIO_FEATURES_SEL_DEVICE);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DRV_FEATURES, 0);

    virtio_status_add(ctx, JLOS_VIRTIO_STATUS_FEATURES_OK);
    if (!(virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS) & JLOS_VIRTIO_STATUS_FEATURES_OK)) {
        printk_err("net features negotiate failed\n");
        return -1;
    }

    if (virtio_net_alloc_queues(ctx) < 0) {
        printk_err("net queue alloc failed\n");
        return -1;
    }
    ctx->rx_last_used = 0;
    ctx->tx_last_used = 0;

    if (virtio_net_setup_rx_queue(ctx) < 0) {
        printk_err("rx queue setup failed\n");
        return -1;
    }
    if (virtio_net_rx_fill(ctx) < 0) {
        printk_err("rx buffers alloc failed\n");
        return -1;
    }
    if (virtio_net_setup_tx_queue(ctx) < 0) {
        printk_err("tx queue setup failed\n");
        return -1;
    }

    uint64_t mac = (uint64_t)virtio_reg_read(ctx, JLOS_VIRTIO_REG_CONFIG)
        | ((uint64_t)(virtio_reg_read(ctx, JLOS_VIRTIO_REG_CONFIG + 4) & 0xFFFF) << 32);
    dev->mac_address = mac;

    virtio_status_add(ctx, JLOS_VIRTIO_STATUS_DRIVER_OK);
    if (virtio_reg_read(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS) & JLOS_VIRTIO_STATUS_FAILED) {
        printk_err("net device entered failed state\n");
        return -1;
    }
    jlos_wmb();
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_NOTIFY, JLOS_VIRTIO_NET_RX_QUEUE);
    return 0;
}

static void virtio_net_dev_send(jlos_hal_net_dev_t *self, uint8_t *buffer, uint32_t size)
{
    jlos_virtio_net_ctx_t *ctx = virtio_net_ctx(self);
    if (!self->inited || size == 0 || size > (JLOS_VIRTIO_NET_BUF_SIZE - JLOS_VIRTIO_NET_HDR_LEN)) {
        return;
    }
    virtio_net_tx_reclaim(ctx);
    uint32_t id = JLOS_VIRTIO_NET_TX_BUFFERS;
    for (uint32_t i = 0; i < JLOS_VIRTIO_NET_TX_BUFFERS; i++) {
        if (!ctx->tx_in_use[i]) {
            id = i;
            break;
        }
    }
    if (id == JLOS_VIRTIO_NET_TX_BUFFERS) {
        return;
    }
    jlos_memcpy(ctx->tx_buffers[id] + JLOS_VIRTIO_NET_HDR_LEN, buffer, size);
    ctx->tx_desc[id].addr = ctx->tx_buffer_phys[id];
    ctx->tx_desc[id].len = JLOS_VIRTIO_NET_HDR_LEN + size;
    ctx->tx_desc[id].flags = 0;
    ctx->tx_desc[id].next = 0;
    ctx->tx_in_use[id] = true;
    ctx->tx_avail->ring[ctx->tx_avail->idx & (ctx->qsize - 1)] = (uint16_t)id;
    jlos_wmb();
    ctx->tx_avail->idx++;
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_QUEUE_NOTIFY, JLOS_VIRTIO_NET_TX_QUEUE);
}

static void virtio_net_dev_set_ip_address(jlos_hal_net_dev_t *self, uint32_t ip)
{
    (void)self;
    (void)ip;
}

static void virtio_net_dev_destroy(jlos_hal_net_dev_t *self)
{
    jlos_virtio_net_ctx_t *ctx = virtio_net_ctx(self);
    virtio_reg_write(ctx, JLOS_VIRTIO_REG_DEVICE_STATUS, 0);
    for (uint16_t i = 0; i < ctx->qsize; i++) {
        if (ctx->rx_buffers[i]) {
            jlos_page_frame_free(ctx->rx_buffers[i]);
        }
    }
    for (uint32_t i = 0; i < JLOS_VIRTIO_NET_TX_BUFFERS; i++) {
        if (ctx->tx_buffers[i]) {
            jlos_page_frame_free(ctx->tx_buffers[i]);
        }
    }
    jlos_page_frame_free(ctx->rx_desc);
    jlos_page_frame_free(ctx->rx_avail);
    jlos_page_frame_free(ctx->rx_used);
    jlos_page_frame_free(ctx->tx_desc);
    jlos_page_frame_free(ctx->tx_avail);
    jlos_page_frame_free(ctx->tx_used);
    self->inited = 0;
}

static const jlos_hal_net_ops_t s_virtio_net_ops = {
    .destroy        = virtio_net_dev_destroy,
    .send           = virtio_net_dev_send,
    .set_ip_address = virtio_net_dev_set_ip_address,
};

static void jlos_virtio_net_dev_init(void)
{
    if (virtio_net_init_hw(&s_virtio_net_ctx, &s_virtio_net_dev) < 0) {
        printk_err("virtio net init failed\n");
        return;
    }
    s_virtio_net_dev.dev_type = JLOS_HAL_NET_DEV_VIRTIO_NET;
    s_virtio_net_dev.ops = &s_virtio_net_ops;
    s_virtio_net_dev.inited = 1;
    *(jlos_virtio_net_ctx_t **)(void *)s_virtio_net_dev.priv = &s_virtio_net_ctx;

    jlos_irq_handler_init(&s_virtio_net_ctx.irq, jlos_active_irq_manager, s_virtio_net_ctx.intid);
    s_virtio_net_ctx.irq.handle_interrupt = virtio_net_handle_irq;

    jlos_hal_net_set_primary_dev(&s_virtio_net_dev);
    printk_info("net0 ready, mac = 0x%lx, intid = %u\n",
        (unsigned long)s_virtio_net_dev.mac_address, s_virtio_net_ctx.intid);
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, jlos_virtio_net_dev_init);
