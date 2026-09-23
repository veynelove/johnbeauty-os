#include <hal/block.h>
#include <hal/diag.h>
#include <drivers/ata.h>

_Static_assert(sizeof(jlos_ata_t) <= JLOS_HAL_BLOCK_PRIV_SIZE, "block priv too small for ata");

static jlos_ata_t *block_ata(jlos_hal_block_dev_t *self)
{
    return (jlos_ata_t *)self->priv;
}

static void ata_pio28_init(jlos_hal_block_dev_t *self)
{
    jlos_ata_init(block_ata(self), block_ata(self)->data_port.portnumber,
                  block_ata(self)->master);
    self->bytes_per_sector = block_ata(self)->bytes_per_sector;
    self->inited = 1;
}

static void ata_pio28_destroy(jlos_hal_block_dev_t *self)
{
    jlos_ata_destroy(block_ata(self));
    self->inited = 0;
}

static void ata_pio28_identify(jlos_hal_block_dev_t *self)
{
    jlos_ata_identify(block_ata(self));
}

static void ata_pio28_flush(jlos_hal_block_dev_t *self)
{ 
    jlos_ata_flush(block_ata(self));
}

static int ata_pio28_read_sectors(jlos_hal_block_dev_t *self, uint64_t lba, uint8_t *buf, uint32_t count)
{
    HAL_TRACE_MSG("ATA PIO-28 read sectors");
    if (!self->inited) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    if (lba > JLOS_ATA28_LBA_MAX || (lba + (uint64_t)count - 1) > JLOS_ATA28_LBA_MAX) {
        return -2;
    }
    uint32_t total_bytes = count * self->bytes_per_sector;
    (void)total_bytes;
    for (uint32_t i = 0; i < count; i++) {
        if (jlos_ata_read28(block_ata(self), (uint32_t)lba + i, buf + i * self->bytes_per_sector,
            (int)self->bytes_per_sector) != 0) {
            return -1;
        }
    }
    return 0;
}

static int ata_pio28_write_sectors(jlos_hal_block_dev_t *self, uint64_t lba,
                                   const uint8_t *buf, uint32_t count)
{
    HAL_TRACE_MSG("ATA PIO-28 write sectors");
    if (!self->inited) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    if (lba > JLOS_ATA28_LBA_MAX || (lba + (uint64_t)count - 1) > JLOS_ATA28_LBA_MAX) {
        return -2;
    }
    for (uint32_t i = 0; i < count; i++) {
        jlos_ata_write28(block_ata(self), (uint32_t)lba + i, (uint8_t *)buf + i * self->bytes_per_sector,
            (int)self->bytes_per_sector);
    }
    return 0;
}

static const jlos_hal_block_ops_t s_ata_pio28_ops = {
    .init           = ata_pio28_init,
    .destroy        = ata_pio28_destroy,
    .identify       = ata_pio28_identify,
    .flush          = ata_pio28_flush,
    .read_sectors   = ata_pio28_read_sectors,
    .write_sectors  = ata_pio28_write_sectors,
};

void jlos_hal_block_ata_pio28_create(jlos_hal_block_dev_t *self, uint16_t port_base, bool master)
{
    if (!self) {
        return;
    }
    self->dev_type = JLOS_HAL_BLOCK_DEV_ATA_PIO28;
    self->bytes_per_sector = 512;
    self->total_sectors = 0;
    self->ops = &s_ata_pio28_ops;
    self->inited = 0;
    jlos_ata_init(block_ata(self), port_base, master);
    jlos_ata_identify(block_ata(self));
    self->bytes_per_sector = block_ata(self)->bytes_per_sector;
    self->total_sectors = block_ata(self)->total_sectors;
    self->inited = (self->total_sectors > 0) ? 1 : 0;
}

void jlos_hal_block_init(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops || !self->ops->init) {
        return;
    }
    self->ops->init(self);
}

void jlos_hal_block_destroy(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops) {
        return;
    }
    if (self->ops->destroy) self->ops->destroy(self);
    self->inited = 0;
}

void jlos_hal_block_identify(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops || !self->ops->identify) {
        return;
    }
    self->ops->identify(self);
}

void jlos_hal_block_flush(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops || !self->ops->flush) {
        return;
    }
    self->ops->flush(self);
}

int jlos_hal_block_read(jlos_hal_block_dev_t *self, uint64_t start_lba,uint8_t *buf, uint32_t sector_count)
{
    if (!self || !self->ops || !self->ops->read_sectors) {
        return -1;
    }
    if (!buf && sector_count > 0) {
        return -1;
    }
    return self->ops->read_sectors(self, start_lba, buf, sector_count);
}

int jlos_hal_block_write(jlos_hal_block_dev_t *self, uint64_t start_lba, const uint8_t *buf, uint32_t sector_count)
{
    if (!self || !self->ops || !self->ops->write_sectors) {
        return -1;
    }
    if (!buf && sector_count > 0) {
        return -1;
    }
    return self->ops->write_sectors(self, start_lba, buf, sector_count);
}
