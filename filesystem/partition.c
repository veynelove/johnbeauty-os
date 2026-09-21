#include <filesystem/partition.h>

#define JLOS_KERNEL_LOG_SUBSYS "ptfs"
#include <kernel/printk.h>

static jlos_partition_dev_t *to_partition(jlos_hal_block_dev_t *dev)
{
    return container_of(dev, jlos_partition_dev_t, base);
}

static void partition_init(jlos_hal_block_dev_t *self)
{
    jlos_partition_dev_t *part = to_partition(self);
    self->bytes_per_sector = part->physical->bytes_per_sector;
    self->total_sectors = part->sector_count;
    self->inited = 1;
}

static void partition_destroy(jlos_hal_block_dev_t *self)
{
    self->inited = 0;
}

static void partition_identify(jlos_hal_block_dev_t *self)
{
    (void)self;
}

static void partition_flush(jlos_hal_block_dev_t *self)
{
    jlos_hal_block_flush(to_partition(self)->physical);
}

static int partition_read_sector(jlos_hal_block_dev_t *self, uint64_t lba, uint8_t *buf, uint32_t count)
{
    if (!self->inited) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    jlos_partition_dev_t *part = to_partition(self);
    return jlos_hal_block_read(part->physical, part->lba_offset + lba, buf, count);
}

static int partition_write_sector(jlos_hal_block_dev_t *self, uint64_t lba, const uint8_t *buf, uint32_t count)
{
    if (!self->inited) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    jlos_partition_dev_t *part = to_partition(self);
    return jlos_hal_block_write(part->physical, part->lba_offset + lba, buf, count);
}

static const jlos_hal_block_ops_t s_partition_ops = {
    .init               = partition_init,
    .destroy            = partition_destroy,
    .identify           = partition_identify,
    .flush              = partition_flush,
    .read_sectors       = partition_read_sector,
    .write_sectors      = partition_write_sector,
};

void jlos_partition_dev_create(jlos_partition_dev_t *self, jlos_hal_block_dev_t *physical, uint64_t lba_offset, uint64_t sector_count)
{
    if (!self || !physical) {
        return;
    }
    self->base.dev_type = JLOS_HAL_BLOCK_DEV_PARTITION;
    self->base.bytes_per_sector = physical->bytes_per_sector;
    self->base.total_sectors = sector_count;
    self->base.ops = &s_partition_ops;
    self->physical = physical;
    self->lba_offset = lba_offset;
    self->sector_count = sector_count;
    self->base.inited = 1;
}

int jlos_partition_parse_mbr(jlos_hal_block_dev_t *dev, jlos_partition_table_entry_t *entries, int max_entries)
{
    if (!dev || !entries || max_entries <= 0) {
        return -1;
    }
    jlos_master_boot_record_t mbr;
    int count = 0;
    if (jlos_hal_block_read(dev, 0, (uint8_t *)&mbr, 1) != 0) {
        printk_err("faild to read mbr\n");
        return -1;
    }
    if (mbr.magicnumber != JLOS_MBR_MAGIC) {
        printk_err("invalid mbr magic: %x\n", mbr.magicnumber);
        return -1;
    }
    for (int i = 0; i < JLOS_MBR_MAX_PARTITIONS && i < max_entries; i++) {
        if (mbr.primary_partition[i].partition_id == 0) {
            continue;
        }
        entries[count] = mbr.primary_partition[i];
        printk_debug("partition %u, type = %x, start_lba = %u, sector = %u\n",
            count & 0xFF, entries[count].partition_id, entries[count].start_lba, entries[count].length);
        count++;
    }
    return count;
}
