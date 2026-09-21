#include <drivers/ata.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "ata"
#include <kernel/printk.h>

static jlos_hal_block_dev_t s_ata_primary_dev;
static bool s_ata_primary_ready = false;

void jlos_ata_init(jlos_ata_t* self, uint16_t port_base, bool master)
{
    jlos_io16_init(&self->data_port, port_base);
    jlos_io8_init(&self->error_port, port_base + 1);
    jlos_io8_init(&self->sector_count_port, port_base + 2);
    jlos_io8_init(&self->lba_low_port, port_base + 3);
    jlos_io8_init(&self->lba_mid_port, port_base + 4);
    jlos_io8_init(&self->lba_hi_port, port_base + 5);
    jlos_io8_init(&self->device_port, port_base + 6);
    jlos_io8_init(&self->command_port, port_base + 7);
    jlos_io8_init(&self->control_port, port_base + 0x206);

    // Disable interrupts (nIEN bit in control register)
    jlos_io8_write(&self->control_port, 0x02);

    self->bytes_per_sector = 512;
    self->master = master;
}

void jlos_ata_destroy(jlos_ata_t* self)
{
    (void)self;
}

void jlos_ata_identify(jlos_ata_t* self)
{
    self->total_sectors = 0;
    jlos_io8_write(&self->control_port, 0x02);
    jlos_io8_write(&self->device_port, self->master ? 0xA0 : 0xB0);
    for (int i = 0; i < 4; i++) {
        (void)jlos_io8_read(&self->command_port);
    }
    uint8_t status = jlos_io8_read(&self->command_port);
    if (status == 0xFF) {
        return;
    }
    jlos_io8_write(&self->sector_count_port, 0);
    jlos_io8_write(&self->lba_low_port, 0);
    jlos_io8_write(&self->lba_mid_port, 0);
    jlos_io8_write(&self->lba_hi_port, 0);
    jlos_io8_write(&self->command_port, 0xEC);
    status = jlos_io8_read(&self->command_port);
    while ((status & 0x80) == 0x80) {
        status = jlos_io8_read(&self->command_port);
    }
    if ((status & 0x01) == 0x01) {
        return;
    }
    if ((status & 0x08) != 0x08) {
        return;
    }
    uint16_t id[256];
    for (int i = 0; i < 256; i++) {
        id[i] = jlos_io16_read(&self->data_port);
    }

    uint32_t lba28 = ((uint32_t)id[61] << 16) | id[60];
    if (lba28 > 0) {
        self->total_sectors = lba28;
    } else {
        self->total_sectors = ((uint32_t)id[58] << 16) | id[57];
    }
}

void jlos_ata_read28(jlos_ata_t* self, uint32_t sector, uint8_t *data, int size)
{
    if (sector & 0xF0000000) {
        return;
    }
    if (size > self->bytes_per_sector) {
        return;
    }
    jlos_io8_write(&self->device_port, (self->master ? 0xE0 : 0xF0) | ((sector & 0xF0000000) >> 24));
    jlos_io8_write(&self->error_port, 0);
    jlos_io8_write(&self->sector_count_port, 1);

    jlos_io8_write(&self->lba_low_port, sector & 0x000000FF);
    jlos_io8_write(&self->lba_mid_port, (sector & 0x0000FF00) >> 8);
    jlos_io8_write(&self->lba_hi_port, (sector & 0x00FF0000) >> 16);
    jlos_io8_write(&self->command_port, 0x20);

    uint8_t status = jlos_io8_read(&self->command_port);
    while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
        status = jlos_io8_read(&self->command_port);
    }
    if (status & 0x01) {
        printk_err("read28 sector %u ERR, status=%x\n", sector, status);
        return;
    }
    if ((status & 0x08) != 0x08) {
        printk_err("read28 sector %u no DRQ, status=%x\n", sector, status);
        return;
    }
    for (uint16_t i = 0; i < size; i += 2) {
        uint16_t wdata = jlos_io16_read(&self->data_port);
        data[i] = wdata & 0x00FF;
        if (i + 1 < size) {
            data[i + 1] = (wdata >> 8) & 0x00FF;
        }
    }

    for (uint16_t i = size + (size % 2); i < self->bytes_per_sector; i += 2) {
        jlos_io16_read(&self->data_port);
    }
}

void jlos_ata_write28(jlos_ata_t* self, uint32_t sector, uint8_t *data, int size)
{
    if (sector & 0xF0000000) {
        return;
    }
    if (size > self->bytes_per_sector) {
        return;
    }
    jlos_io8_write(&self->device_port, (self->master ? 0xE0 : 0xF0) | ((sector & 0xF0000000) >> 24));
    jlos_io8_write(&self->error_port, 0);
    jlos_io8_write(&self->sector_count_port, 1);

    jlos_io8_write(&self->lba_low_port, sector & 0x000000FF);
    jlos_io8_write(&self->lba_mid_port, (sector & 0x0000FF00) >> 8);
    jlos_io8_write(&self->lba_hi_port, (sector & 0x00FF0000) >> 16);
    jlos_io8_write(&self->command_port, 0x30);

    for (uint16_t i = 0; i < size; i += 2) {
        uint16_t wdata = data[i];
        if (i + 1 < size) {
            wdata |= ((uint16_t)data[i + 1]) << 8;
        }
        jlos_io16_write(&self->data_port, wdata);
    }
    for (uint16_t i = size + (size % 2); i < self->bytes_per_sector; i += 2) {
        jlos_io16_write(&self->data_port, 0x0000);
    }
}

void jlos_ata_flush(jlos_ata_t* self)
{
    jlos_io8_write(&self->device_port, self->master ? 0xE0 : 0xF0);
    jlos_io8_write(&self->command_port, 0xE7);

    uint8_t status = jlos_io8_read(&self->command_port);
    while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
        status = jlos_io8_read(&self->command_port);
    }
}

jlos_hal_block_dev_t *jlos_ata_get_primary_dev(void)
{
    return s_ata_primary_ready ? &s_ata_primary_dev : NULL;
}

static void jlos_ata_block_dev_init(void)
{
    jlos_hal_block_ata_pio28_create(&s_ata_primary_dev, jlos_ata_primary_port_base, true);
    if (s_ata_primary_dev.inited) {
        s_ata_primary_ready = true;
        printk_debug("ata primary device ready, sectors = %u\n", (unsigned long long)s_ata_primary_dev.total_sectors);
    } else {
        printk_err("ata primary device init failed\n");
    }
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, jlos_ata_block_dev_init);
