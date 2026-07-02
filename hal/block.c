#include <hal/block.h>
#include <hal/diag.h>

/* ---------------- ATA PIO-28 后端：具体 ops ---------------- */

static void ata_pio28_init(jlos_hal_block_dev_t *self)
{
    /* create 时已经填好 priv.ata 里 port_base + master，这里做硬件 init */
    jlos_ata_init(&self->dev_priv.ata, self->dev_priv.ata.m_data_port.m_portnumber,
                  self->dev_priv.ata.m_master);
    self->bytes_per_sector = self->dev_priv.ata.m_bytes_per_sector;
    self->inited = 1;
}

static void ata_pio28_destroy(jlos_hal_block_dev_t *self)
{
    jlos_ata_destroy(&self->dev_priv.ata);
    self->inited = 0;
}

static void ata_pio28_identify(jlos_hal_block_dev_t *self)
{
    jlos_ata_identify(&self->dev_priv.ata);
    /* ATA identify 返回的扇区数在 IDENTIFY 数据 offset 60-61（word），
     * 这里先不解析（ATA 驱动可能没返回），留 total_sectors=0，上层知道是未识别 */
}

static void ata_pio28_flush(jlos_hal_block_dev_t *self)
{ jlos_ata_flush(&self->dev_priv.ata); }

static int ata_pio28_read_sectors(jlos_hal_block_dev_t *self, uint64_t lba,
                                  uint8_t *buf, uint32_t count)
{
    HAL_TRACE_MSG("ATA PIO-28 read sectors");
    if (!self->inited) return -1;
    if (count == 0) return 0;
    /* PIO-28 只支持 28-bit LBA（最大 2^28-1 sectors ≈ 128GB） */
    if (lba > 0x0FFFFFFFull || (lba + (uint64_t)count - 1) > 0x0FFFFFFFull) return -2;
    /* ATA read28 API: m_size = bytes，所以 count * bytes_per_sector */
    uint32_t total_bytes = count * self->bytes_per_sector;
    /* 连续跨扇区的正确性依赖 ATA 驱动本身，这里只做字节上限
     * jlos_ata_read28 一次只读 count=1？还是支持多扇区？查 ata.h 签名是：
     *   void jlos_ata_read28(..., sector, *data, m_size)
     * 不管怎样我们按扇区循环，保守策略。 */
    for (uint32_t i = 0; i < count; i++) {
        jlos_ata_read28(&self->dev_priv.ata,
                        (uint32_t)lba + i,
                        buf + i * self->bytes_per_sector,
                        (int)self->bytes_per_sector);
    }
    return 0;
}

static int ata_pio28_write_sectors(jlos_hal_block_dev_t *self, uint64_t lba,
                                   const uint8_t *buf, uint32_t count)
{
    HAL_TRACE_MSG("ATA PIO-28 write sectors");
    if (!self->inited) return -1;
    if (count == 0) return 0;
    if (lba > 0x0FFFFFFFull || (lba + (uint64_t)count - 1) > 0x0FFFFFFFull) return -2;
    for (uint32_t i = 0; i < count; i++) {
        /* const 去掉：ATA write28 API 没有声明 const，但实际 write 不会改 buf */
        jlos_ata_write28(&self->dev_priv.ata,
                         (uint32_t)lba + i,
                         (uint8_t *)buf + i * self->bytes_per_sector,
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

/* ---------------- ATA PIO-28 构造函数 ---------------- */

void jlos_hal_block_ata_pio28_create(jlos_hal_block_dev_t *self,
                                     uint16_t port_base, bool master)
{
    if (!self) return;
    self->dev_type = JLOS_HAL_BLOCK_DEV_ATA_PIO28;
    self->bytes_per_sector = 512;
    self->total_sectors = 0;
    self->ops = &s_ata_pio28_ops;
    self->inited = 0;
    /* jlos_ata_init 会把 port_base 填到 m_data_port + 各端口里，这里先让
     * m_data_port.m_portnumber = port_base，让 init 时能用；其余字段由 ata_init 填。
     * 因为 jlos_ata_init 签名是 init(self, port_base, master)，我们不需要手动填太多。 */
    (void)port_base; (void)master;
    /* 但 master 和 port_base 要传给 jlos_ata_init，我们用 ata 对象的位置先存一份？
     * 简单：调用 ata_init 之前 jlos_ata_t 里的信息没用，直接在 create 里先 ata_init
     * 就好了，这也符合 HAL "create 后即 init 好" 的语义。 */
    jlos_ata_init(&self->dev_priv.ata, port_base, master);
    self->bytes_per_sector = self->dev_priv.ata.m_bytes_per_sector;
    self->inited = 1;
}

/* ---------------- 通用块设备 API ---------------- */

void jlos_hal_block_init(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops || !self->ops->init) return;
    self->ops->init(self);
}

void jlos_hal_block_destroy(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops) return;
    if (self->ops->destroy) self->ops->destroy(self);
    self->inited = 0;
}

void jlos_hal_block_identify(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops || !self->ops->identify) return;
    self->ops->identify(self);
}

void jlos_hal_block_flush(jlos_hal_block_dev_t *self)
{
    if (!self || !self->ops || !self->ops->flush) return;
    self->ops->flush(self);
}

int jlos_hal_block_read(jlos_hal_block_dev_t *self, uint64_t start_lba,
                        uint8_t *buf, uint32_t sector_count)
{
    if (!self || !self->ops || !self->ops->read_sectors) return -1;
    if (!buf && sector_count > 0) return -1;
    return self->ops->read_sectors(self, start_lba, buf, sector_count);
}

int jlos_hal_block_write(jlos_hal_block_dev_t *self, uint64_t start_lba,
                         const uint8_t *buf, uint32_t sector_count)
{
    if (!self || !self->ops || !self->ops->write_sectors) return -1;
    if (!buf && sector_count > 0) return -1;
    return self->ops->write_sectors(self, start_lba, buf, sector_count);
}
