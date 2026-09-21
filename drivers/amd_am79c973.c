#include <drivers/amd_am79c973.h>
#include <kernel/memory_manager.h>

#define JLOS_KERNEL_LOG_SUBSYS "eth"
#include <kernel/printk.h>

void jlos_rawdata_handler_init(jlos_rawdata_handler_t* self, jlos_amd_am79c973_t *backend)
{
    self->backend = backend;
    self->on_raw_data_received = jlos_rawdata_handler_on_raw_data_received;
    self->send = jlos_rawdata_handler_send;
    jlos_amd_am79c973_set_handler(backend, self);
}

void jlos_rawdata_handler_destroy(jlos_rawdata_handler_t* self)
{
    jlos_amd_am79c973_set_handler(self->backend, NULL);
}

bool jlos_rawdata_handler_on_raw_data_received(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t size)
{
    (void)self;
    (void)buffer;
    (void)size;
    return false;
}

void jlos_rawdata_handler_send(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t size)
{
    (void)self;
    (void)buffer;
    (void)size;
}

void jlos_amd_am79c973_init(jlos_amd_am79c973_t* self, jlos_hal_pci_device_t *dev, jlos_irq_manager_t *interrupts)
{
    jlos_driver_init(&self->base_driver);
    self->base_driver.activate = (void (*)(jlos_driver_t*))jlos_amd_am79c973_activate;

    uint8_t irq = dev->interrupt;
    uint8_t interrupt_number = irq + jlos_irq_manager_hw_offset(interrupts);
    printk_info("IRQ=%x interrupt=%x\n", irq, interrupt_number);
    jlos_irq_handler_init(&self->base_handler, interrupts, interrupt_number);
    self->base_handler.handle_interrupt = (jlos_irq_handler_func_t)jlos_amd_am79c973_handle_interrupt;

    jlos_io16_init(&self->mac_address0_port, dev->port_base);
    jlos_io16_init(&self->mac_address2_port, dev->port_base + 0x02);
    jlos_io16_init(&self->mac_address4_port, dev->port_base + 0x04);
    jlos_io16_init(&self->register_data_port, dev->port_base + 0x10);
    jlos_io16_init(&self->register_address_port, dev->port_base + 0x12);
    jlos_io16_init(&self->reset_port, dev->port_base + 0x14);
    jlos_io16_init(&self->bus_control_register_data_port, dev->port_base + 0x16);

    self->handler = NULL;
    self->current_send_buffer = 0;
    self->current_recv_buffer = 0;

    uint16_t mac_port0 = jlos_io16_read(&self->mac_address0_port);
    uint16_t mac_port2 = jlos_io16_read(&self->mac_address2_port);
    uint16_t mac_port4 = jlos_io16_read(&self->mac_address4_port);
    printk_debug("MAC port registers: %X %X %X %X %X %X\n",
        (mac_port0 >> 8) & 0xFF, mac_port0 & 0xFF,
        (mac_port2 >> 8) & 0xFF, mac_port2 & 0xFF,
        (mac_port4 >> 8) & 0xFF, mac_port4 & 0xFF);
    uint64_t MAC0 = mac_port0 % 256;
    uint64_t MAC1 = mac_port0 / 256;
    uint64_t MAC2 = mac_port2 % 256;
    uint64_t MAC3 = mac_port2 / 256;
    uint64_t MAC4 = mac_port4 % 256;
    uint64_t MAC5 = mac_port4 / 256;

    uint8_t *init_block_raw = (uint8_t*)((((uint32_t)&self->init_block_memory[0]) + 31) & ~((uint32_t)0x1F));
    self->init_block = (jlos_amd_init_block_t*)init_block_raw;

    self->init_block->mode = 0x0430;
    {
        uint8_t recv_code = 0, send_code = 0;
        uint32_t tmp = NUM_RECV_BUFFERS;
        while (tmp > 1) { tmp >>= 1; recv_code++; }
        tmp = NUM_SEND_BUFFERS;
        while (tmp > 1) { tmp >>= 1; send_code++; }
        self->init_block->recv_len = (recv_code << 4);
        self->init_block->send_len = (send_code << 4);
        printk_debug("init RLEN=0x%x (%u recv) SLEN=0x%x (%u send)\n",
            self->init_block->recv_len, NUM_RECV_BUFFERS,
            self->init_block->send_len, NUM_SEND_BUFFERS);
    }
    self->init_block->reserved = 0x0000;
    self->init_block->physical_address[0] = MAC0;
    self->init_block->physical_address[1] = MAC1;
    self->init_block->physical_address[2] = MAC2;
    self->init_block->physical_address[3] = MAC3;
    self->init_block->physical_address[4] = MAC4;
    self->init_block->physical_address[5] = MAC5;
    for (int i = 0; i < 8; i++) {
        self->init_block->logical_address[i] = 0;
    }

    self->send_buffer_descr = (jlos_amd_buffer_descriptor_t *)(((uint32_t)(&self->send_buffer_desc_memory[0]) + 15) & ~((uint32_t)0xF));
    self->recv_buffer_descr = (jlos_amd_buffer_descriptor_t *)(((uint32_t)(&self->recv_buffer_desc_memory[0]) + 15) & ~((uint32_t)0xF));

    jlos_memset(self->send_buffer_descr, 0, NUM_SEND_BUFFERS * sizeof(jlos_amd_buffer_descriptor_t));
    jlos_memset(self->recv_buffer_descr, 0, NUM_RECV_BUFFERS * sizeof(jlos_amd_buffer_descriptor_t));

    self->init_block->recv_buffer_descr_address = (uint32_t)self->recv_buffer_descr;
    self->init_block->send_buffer_descr_address = (uint32_t)self->send_buffer_descr;

    uint32_t send_buffers_base = (((uint32_t)&self->send_buffers[0][0]) + 0x7FF) & ~((uint32_t)0x7FF);
    uint32_t recv_buffers_base = (((uint32_t)&self->recv_buffers[0][0]) + 0x7FF) & ~((uint32_t)0x7FF);
    (void)send_buffers_base;
    (void)recv_buffers_base;

    uint32_t buffer_size_bs = (2048 / 256) << 16;
    for (uint8_t i = 0; i < NUM_SEND_BUFFERS; i++) {
        self->send_buffer_descr[i].address = (uint32_t)&self->send_buffers[i][0];
        self->send_buffer_descr[i].flags = buffer_size_bs;
        self->send_buffer_descr[i].flags2 = 0;
        self->send_buffer_descr[i].avail = 0x8000;
        self->send_buffer_descr[i].reserved = 0;
    }
    for (uint8_t i = 0; i < NUM_RECV_BUFFERS; i++) {
        self->recv_buffer_descr[i].address = (uint32_t)&self->recv_buffers[i][0];
        self->recv_buffer_descr[i].flags = (0x80000000 | buffer_size_bs | 0xF800);
        self->recv_buffer_descr[i].flags2 = 0;
        self->recv_buffer_descr[i].avail = 0x8000;
        self->recv_buffer_descr[i].reserved = 0;
    }

    jlos_irq_manager_register(interrupts, dev->interrupt + jlos_irq_manager_hw_offset(interrupts), &self->base_handler);
}

void jlos_amd_am79c973_destroy(jlos_amd_am79c973_t* self)
{
    (void)self;
}

void jlos_amd_am79c973_activate(jlos_amd_am79c973_t* self)
{
    uint32_t temp;

    jlos_io16_write(&self->register_address_port, 20);
    jlos_io16_write(&self->bus_control_register_data_port, 0x102);
    printk_debug("BCR written with 0x102\n");
    jlos_io16_write(&self->register_address_port, 0);
    jlos_io16_write(&self->register_data_port, 0x04);
    printk_debug("CSR0 written with 0x04 (STOP)\n");
    for (int i = 0; i < 10000; i++) {
        jlos_io16_write(&self->register_address_port, 0);
        temp = jlos_io16_read(&self->register_data_port);
        if ((temp & 0x0008) == 0) {
            break;
        }
    }
    printk_info("STOP acknowledged\n");

    jlos_io16_write(&self->register_address_port, 4);
    temp = jlos_io16_read(&self->register_data_port);
    jlos_io16_write(&self->register_data_port, (temp & 0x0FFF) | 0x0115);

    uint32_t init_block_addr = (uint32_t)self->init_block;
    jlos_io16_write(&self->register_address_port, 1);
    jlos_io16_write(&self->register_data_port, init_block_addr & 0xFFFF);
    jlos_io16_write(&self->register_address_port, 2);
    jlos_io16_write(&self->register_data_port, (init_block_addr >> 16) & 0xFFFF);

    jlos_io16_write(&self->register_address_port, 0);
    jlos_io16_write(&self->register_data_port, 0x01);

    for (int i = 0; i < 10000; i++) {
        jlos_io16_write(&self->register_address_port, 0);
        temp = jlos_io16_read(&self->register_data_port);
        if ((temp & 0x0100) != 0) {
            break;
        }
    }

    jlos_io16_write(&self->register_address_port, 0);
    jlos_io16_write(&self->register_data_port, 0x42);

    {
        jlos_io16_write(&self->register_address_port, 0);
        uint16_t csr0_after = jlos_io16_read(&self->register_data_port);
        printk_debug("post-start CSR0=0x%x STRT=%x INEA=%x INTR=%x RXON=%x TXON=%x RINT=%x TINT=%x IDON=%x\n",
            csr0_after,
            (csr0_after & 0x0002) ? 1 : 0,
            (csr0_after & 0x0040) ? 1 : 0,
            (csr0_after & 0x0080) ? 1 : 0,
            (csr0_after & 0x0020) ? 1 : 0,
            (csr0_after & 0x0010) ? 1 : 0,
            (csr0_after & 0x0400) ? 1 : 0,
            (csr0_after & 0x0200) ? 1 : 0,
            (csr0_after & 0x0100) ? 1 : 0);
    }

    printk_info("activation complete\n");
}

uint32_t jlos_amd_am79c973_handle_interrupt(jlos_irq_handler_t* handler, uint32_t esp)
{
    jlos_amd_am79c973_t* eth = container_of(handler, jlos_amd_am79c973_t, base_handler);

    jlos_io16_write(&eth->register_address_port, 0);
    uint32_t temp = jlos_io16_read(&eth->register_data_port);
    uint16_t command = temp & 0x00C6;

    if ((temp & 0x8000) == 0x8000) {
        command |= 0x8000;
    }
    if ((temp & 0x4000) == 0x4000) {
        printk_err("babble error\n");
        command |= 0x4000;
    }
    if ((temp & 0x2000) == 0x2000) {
        printk_err("collision error\n");
        command |= 0x2000;
    }
    if ((temp & 0x1000) == 0x1000) {
        printk_err("miss error\n");
        command |= 0x1000;
    }
    if ((temp & 0x0800) == 0x0800) {
        printk_err("memory error\n");
        command |= 0x0800;
    }
    if ((temp & 0x0400) == 0x0400) {
        jlos_amd_am79c973_receive(eth);
        command |= 0x0400;
    }
    if ((temp & 0x0200) == 0x0200) {
        command |= 0x0200;
    }
    if ((temp & 0x0100) == 0x0100) {
        printk_info("init done\n");
        command |= 0x0100;
    }

    jlos_io16_write(&eth->register_address_port, 0);
    jlos_io16_write(&eth->register_data_port, command);

    return esp;
}

void jlos_amd_am79c973_send(jlos_amd_am79c973_t* self, uint8_t *buffer, int size)
{
    int send_descriptor = self->current_send_buffer;
    self->current_send_buffer = (self->current_send_buffer + 1) % NUM_SEND_BUFFERS;
    if (size > 1518) {
        size = 1518;
    }
    for (uint8_t *src = buffer + size - 1, *dst =
         (uint8_t *)(self->send_buffer_descr[send_descriptor].address + size - 1);
         src >= buffer; src--, dst--)
    {
        *dst = *src;
    }
    printk_debug("send: %d bytes\n", size);
    self->send_buffer_descr[send_descriptor].avail = 0x8000;
    self->send_buffer_descr[send_descriptor].flags2 = 0;
    self->send_buffer_descr[send_descriptor].reserved = 0;
    uint32_t send_bs = (2048 / 256) << 16;
    self->send_buffer_descr[send_descriptor].flags = 0x80000000 | 0x03000000 | send_bs | ((uint16_t)((-size) & 0xFFF));
    jlos_io16_write(&self->register_address_port, 0);
    jlos_io16_write(&self->register_data_port, 0x48);

    {
        uint32_t sflags = 0;
        int sent = 0;
        for (volatile int delay = 0; delay < 500000; delay++) {
            __asm__("nop");
            sflags = self->send_buffer_descr[send_descriptor].flags;
            if ((sflags & 0x80000000) == 0) { sent = 1; break; }
        }
        jlos_io16_write(&self->register_address_port, 0);
        uint16_t csr0_send = jlos_io16_read(&self->register_data_port);
        printk_debug("post-send CSR0=0x%x RINT=%x TINT=%x INTR=%x INEA=%x RXON=%x TXON=%x sent=%x sflags=0x%x\n",
            csr0_send,
            (csr0_send & 0x0400) ? 1 : 0,
            (csr0_send & 0x0200) ? 1 : 0,
            (csr0_send & 0x0080) ? 1 : 0,
            (csr0_send & 0x0040) ? 1 : 0,
            (csr0_send & 0x0020) ? 1 : 0,
            (csr0_send & 0x0010) ? 1 : 0,
            sent,
            sflags);
    }
}

void jlos_amd_am79c973_receive(jlos_amd_am79c973_t* self)
{
    uint32_t bs = (2048 / 256) << 16;
    uint8_t start_idx = self->current_recv_buffer;
    int8_t last_processed_iter = -1;

    for (int i = 0; i < NUM_RECV_BUFFERS; i++) {
        int idx = (start_idx + i) % NUM_RECV_BUFFERS;
        uint32_t flags = self->recv_buffer_descr[idx].flags;
        uint32_t flags2 = self->recv_buffer_descr[idx].flags2;
        uint16_t avail = self->recv_buffer_descr[idx].avail;
        (void)avail;

        if ((flags & 0x80000000) != 0) {
            continue;
        }

        uint32_t stp_enp_ok = 0;
        if ((flags & 0x00C00000) == 0x00C00000) {
            stp_enp_ok = 1;
        }
        if ((flags & 0x03000000) == 0x03000000) {
            stp_enp_ok = 2;
        }

        if (stp_enp_ok) {
            uint32_t size = flags2 & 0xFFF;
            if (size > 64) {
                size -= 4;
            }
            uint8_t *buffer = (uint8_t *)(self->recv_buffer_descr[idx].address);
            if (self->handler) {
                if (self->handler->on_raw_data_received(self->handler, buffer, size)) {
                    jlos_amd_am79c973_send(self, buffer, size);
                }
            }
        }
        self->recv_buffer_descr[idx].flags = (0x80000000 | bs | 0xF800);
        self->recv_buffer_descr[idx].flags2 = 0;
        self->recv_buffer_descr[idx].avail = 0x8000;
        self->recv_buffer_descr[idx].reserved = 0;
        last_processed_iter = i;
    }

    if (last_processed_iter >= 0) {
        self->current_recv_buffer = (start_idx + last_processed_iter + 1) % NUM_RECV_BUFFERS;
    }
}

void jlos_amd_am79c973_set_handler(jlos_amd_am79c973_t* self, jlos_rawdata_handler_t *handler)
{
    self->handler = handler;
}

uint64_t jlos_amd_am79c973_get_mac_address(jlos_amd_am79c973_t* self)
{
    uint64_t mac = 0;
    for (int i = 0; i < 6; i++) {
        mac |= ((uint64_t)self->init_block->physical_address[i]) << (8 * i);
    }
    return mac;
}

void jlos_amd_am79c973_set_ip_address(jlos_amd_am79c973_t* self, uint32_t ip)
{
    for (int i = 0; i < 4; i++) {
        self->init_block->logical_address[i] = (ip >> (8 * i)) & 0xFF;
    }
    for (int i = 4; i < 8; i++) {
        self->init_block->logical_address[i] = 0;
    }
}

uint32_t jlos_amd_am79c973_get_ip_address(jlos_amd_am79c973_t* self)
{
    uint32_t ip = 0;
    for (int i = 0; i < 4; i++) {
        ip |= ((uint32_t)self->init_block->logical_address[i]) << (8 * i);
    }
    return ip;
}
