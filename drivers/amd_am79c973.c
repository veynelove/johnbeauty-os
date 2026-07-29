#include <drivers/amd_am79c973.h>
#include <kernel/memory_manager.h>

extern void printf(const char *str);
extern void printf_hex(uint8_t);
extern void printf_hex32(uint32_t);

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

bool jlos_rawdata_handler_on_raw_data_received(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t m_size)
{
    return false;
}

void jlos_rawdata_handler_send(jlos_rawdata_handler_t* self, uint8_t *buffer, uint32_t m_size)
{
}

void jlos_amd_am79c973_init(jlos_amd_am79c973_t* self, jlos_pci_device_descriptor_t *dev, jlos_irq_manager_t *interrupts)
{
    jlos_driver_init(&self->base_driver);
    self->base_driver.activate = (void (*)(jlos_driver_t*))jlos_amd_am79c973_activate;
    self->base_driver.reset = (int (*)(jlos_driver_t*))jlos_amd_am79c973_reset;
    
    uint8_t irq = dev->m_interrupt;
    uint8_t interrupt_number = irq + jlos_irq_manager_hw_offset(interrupts);
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("AMD am79c973 IRQ=");
    printf_hex(irq);
    printf(" interrupt=");
    printf_hex(interrupt_number);
    printf("\n");
#endif
    jlos_irq_handler_init(&self->base_handler, interrupts, interrupt_number);
    self->base_handler.handle_interrupt = (jlos_irq_handler_func_t)jlos_amd_am79c973_handle_interrupt;
    
    jlos_io16_init(&self->m_mac_address0_port, dev->m_port_base);
    jlos_io16_init(&self->m_mac_address2_port, dev->m_port_base + 0x02);
    jlos_io16_init(&self->m_mac_address4_port, dev->m_port_base + 0x04);
    jlos_io16_init(&self->m_register_data_port, dev->m_port_base + 0x10);
    jlos_io16_init(&self->m_register_address_port, dev->m_port_base + 0x12);
    jlos_io16_init(&self->m_reset_port, dev->m_port_base + 0x14);
    jlos_io16_init(&self->m_bus_control_register_data_port, dev->m_port_base + 0x16);

    self->handler = NULL;
    self->m_current_send_buffer = 0;
    self->m_current_recv_buffer = 0;

    uint16_t mac_port0 = jlos_io16_read(&self->m_mac_address0_port);
    uint16_t mac_port2 = jlos_io16_read(&self->m_mac_address2_port);
    uint16_t mac_port4 = jlos_io16_read(&self->m_mac_address4_port);
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("MAC port registers: ");
    printf_hex((mac_port0 >> 8) & 0xFF);
    printf_hex(mac_port0 & 0xFF);
    printf(" ");
    printf_hex((mac_port2 >> 8) & 0xFF);
    printf_hex(mac_port2 & 0xFF);
    printf(" ");
    printf_hex((mac_port4 >> 8) & 0xFF);
    printf_hex(mac_port4 & 0xFF);
    printf("\n");
#endif
    uint64_t MAC0 = mac_port0 % 256;
    uint64_t MAC1 = mac_port0 / 256;
    uint64_t MAC2 = mac_port2 % 256;
    uint64_t MAC3 = mac_port2 / 256;
    uint64_t MAC4 = mac_port4 % 256;
    uint64_t MAC5 = mac_port4 / 256;

    uint8_t *init_block_raw = (uint8_t*)((((uint32_t)&self->init_block_memory[0]) + 31) & ~((uint32_t)0x1F));
    self->m_init_block = (jlos_amd_init_block_t*)init_block_raw;
    
    self->m_init_block->m_mode = 0x0430;
    {
        uint8_t recv_code = 0, send_code = 0;
        uint32_t tmp = NUM_RECV_BUFFERS;
        while (tmp > 1) { tmp >>= 1; recv_code++; }
        tmp = NUM_SEND_BUFFERS;
        while (tmp > 1) { tmp >>= 1; send_code++; }
        self->m_init_block->m_recv_len = (recv_code << 4);
        self->m_init_block->m_send_len = (send_code << 4);
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("INIT RLEN=0x");
        printf_hex(self->m_init_block->m_recv_len);
        printf(" (");
        printf_hex(NUM_RECV_BUFFERS);
        printf(" recv) SLEN=0x");
        printf_hex(self->m_init_block->m_send_len);
        printf(" (");
        printf_hex(NUM_SEND_BUFFERS);
        printf(" send)\n");
#endif
    }
    self->m_init_block->m_reserved = 0x0000;
    self->m_init_block->physical_address[0] = MAC0;
    self->m_init_block->physical_address[1] = MAC1;
    self->m_init_block->physical_address[2] = MAC2;
    self->m_init_block->physical_address[3] = MAC3;
    self->m_init_block->physical_address[4] = MAC4;
    self->m_init_block->physical_address[5] = MAC5;
    for (int i = 0; i < 8; i++) {
        self->m_init_block->m_logical_address[i] = 0;
    }

    self->send_buffer_descr = (jlos_amd_buffer_descriptor_t *)(((uint32_t)(&self->send_buffer_desc_memory[0]) + 15) & ~((uint32_t)0xF));
    self->recv_buffer_descr = (jlos_amd_buffer_descriptor_t *)(((uint32_t)(&self->recv_buffer_desc_memory[0]) + 15) & ~((uint32_t)0xF));

    jlos_memset(self->send_buffer_descr, 0, NUM_SEND_BUFFERS * sizeof(jlos_amd_buffer_descriptor_t));
    jlos_memset(self->recv_buffer_descr, 0, NUM_RECV_BUFFERS * sizeof(jlos_amd_buffer_descriptor_t));

    self->m_init_block->m_recv_buffer_descr_address = (uint32_t)self->recv_buffer_descr;
    self->m_init_block->m_send_buffer_descr_address = (uint32_t)self->send_buffer_descr;

    uint32_t send_buffers_base = (((uint32_t)&self->send_buffers[0][0]) + 0x7FF) & ~((uint32_t)0x7FF);
    uint32_t recv_buffers_base = (((uint32_t)&self->recv_buffers[0][0]) + 0x7FF) & ~((uint32_t)0x7FF);

    uint32_t buffer_size_bs = (2048 / 256) << 16;
    for (uint8_t i = 0; i < NUM_SEND_BUFFERS; i++) {
        self->send_buffer_descr[i].m_address = (uint32_t)&self->send_buffers[i][0];
        self->send_buffer_descr[i].m_flags = buffer_size_bs;
        self->send_buffer_descr[i].m_flags2 = 0;
        self->send_buffer_descr[i].m_avail = 0x8000;
        self->send_buffer_descr[i].m_reserved = 0;
    }
    for (uint8_t i = 0; i < NUM_RECV_BUFFERS; i++) {
        self->recv_buffer_descr[i].m_address = (uint32_t)&self->recv_buffers[i][0];
        self->recv_buffer_descr[i].m_flags = (0x80000000 | buffer_size_bs | 0xF800);
        self->recv_buffer_descr[i].m_flags2 = 0;
        self->recv_buffer_descr[i].m_avail = 0x8000;
        self->recv_buffer_descr[i].m_reserved = 0;
    }
    
    jlos_irq_manager_register(interrupts, dev->m_interrupt + jlos_irq_manager_hw_offset(interrupts), &self->base_handler);
}

void jlos_amd_am79c973_destroy(jlos_amd_am79c973_t* self)
{
}

void jlos_amd_am79c973_activate(jlos_amd_am79c973_t* self)
{
    uint32_t temp;

    jlos_io16_write(&self->m_register_address_port, 20);
    jlos_io16_write(&self->m_bus_control_register_data_port, 0x102);
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("BCR written with 0x102\n");
#endif
    jlos_io16_write(&self->m_register_address_port, 0);
    jlos_io16_write(&self->m_register_data_port, 0x04);
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("CSR0 written with 0x04 (STOP)\n");
#endif
    for (int i = 0; i < 10000; i++) {
        jlos_io16_write(&self->m_register_address_port, 0);
        temp = jlos_io16_read(&self->m_register_data_port);
        if ((temp & 0x0008) == 0) {
            break;
        }
    }
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("STOP acknowledged\n");
#endif

    jlos_io16_write(&self->m_register_address_port, 4);
    temp = jlos_io16_read(&self->m_register_data_port);
    jlos_io16_write(&self->m_register_data_port, (temp & 0x0FFF) | 0x0115);

    uint32_t init_block_addr = (uint32_t)self->m_init_block;
    jlos_io16_write(&self->m_register_address_port, 1);
    jlos_io16_write(&self->m_register_data_port, init_block_addr & 0xFFFF);
    jlos_io16_write(&self->m_register_address_port, 2);
    jlos_io16_write(&self->m_register_data_port, (init_block_addr >> 16) & 0xFFFF);

    jlos_io16_write(&self->m_register_address_port, 0);
    jlos_io16_write(&self->m_register_data_port, 0x01);

    for (int i = 0; i < 10000; i++) {
        jlos_io16_write(&self->m_register_address_port, 0);
        temp = jlos_io16_read(&self->m_register_data_port);
        if ((temp & 0x0100) != 0) {
            break;
        }
    }

    jlos_io16_write(&self->m_register_address_port, 0);
    jlos_io16_write(&self->m_register_data_port, 0x42);

#if KERNEL_CONFIG_DEBUG_NETWORK
    {
        jlos_io16_write(&self->m_register_address_port, 0);
        uint16_t csr0_after = jlos_io16_read(&self->m_register_data_port);
        printf("POST-START CSR0=0x");
        printf_hex((csr0_after >> 8) & 0xFF);
        printf_hex(csr0_after & 0xFF);
        printf(" STRT=");
        printf_hex((csr0_after & 0x0002) ? 1 : 0);
        printf(" INEA=");
        printf_hex((csr0_after & 0x0040) ? 1 : 0);
        printf(" INTR=");
        printf_hex((csr0_after & 0x0080) ? 1 : 0);
        printf(" RXON=");
        printf_hex((csr0_after & 0x0020) ? 1 : 0);
        printf(" TXON=");
        printf_hex((csr0_after & 0x0010) ? 1 : 0);
        printf(" RINT=");
        printf_hex((csr0_after & 0x0400) ? 1 : 0);
        printf(" TINT=");
        printf_hex((csr0_after & 0x0200) ? 1 : 0);
        printf(" IDON=");
        printf_hex((csr0_after & 0x0100) ? 1 : 0);
        printf("\n");
    }
#endif

#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("AMD am79c973 activation complete\n");
#endif
}

int jlos_amd_am79c973_reset(jlos_amd_am79c973_t* self)
{
    jlos_io16_read(&self->m_reset_port);
    jlos_io16_write(&self->m_reset_port, 0);
    return 10;
}

uint32_t jlos_amd_am79c973_handle_interrupt(jlos_irq_handler_t* handler, uint32_t m_esp)
{
#define offsetof(type, member) ((size_t)((char*)&((type*)0)->member))
    jlos_amd_am79c973_t* eth = (jlos_amd_am79c973_t*)((char*)handler - offsetof(jlos_amd_am79c973_t, base_handler));

    jlos_io16_write(&eth->m_register_address_port, 0);
    uint32_t temp = jlos_io16_read(&eth->m_register_data_port);
    uint16_t command = temp & 0x00C6;

    if ((temp & 0x8000) == 0x8000) {
        command |= 0x8000;
    }
    if ((temp & 0x4000) == 0x4000) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("AMD am79c973 BABBLE ERROR\n");
#endif
        command |= 0x4000;
    }
    if ((temp & 0x2000) == 0x2000) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("AMD am79c973 COLLISION ERROR\n");
#endif
        command |= 0x2000;
    }
    if ((temp & 0x1000) == 0x1000) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("AMD am79c973 MISS ERROR\n");
#endif
        command |= 0x1000;
    }
    if ((temp & 0x0800) == 0x0800) {
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("AMD am79c973 MEMORY ERROR\n");
#endif
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
#if KERNEL_CONFIG_DEBUG_NETWORK
        printf("AMD am79c973 INIT DONE\n");
#endif
        command |= 0x0100;
    }

    jlos_io16_write(&eth->m_register_address_port, 0);
    jlos_io16_write(&eth->m_register_data_port, command);

    return m_esp;
}

void jlos_amd_am79c973_send(jlos_amd_am79c973_t* self, uint8_t *buffer, int m_size)
{
    int send_descriptor = self->m_current_send_buffer;
    self->m_current_send_buffer = (self->m_current_send_buffer + 1) % NUM_SEND_BUFFERS;
    if (m_size > 1518) {
        m_size = 1518;
    }
    for (uint8_t *src = buffer + m_size - 1, *dst =
         (uint8_t *)(self->send_buffer_descr[send_descriptor].m_address + m_size - 1);
         src >= buffer; src--, dst--)
    {
        *dst = *src;
    }
#if KERNEL_CONFIG_DEBUG_NETWORK
    printf("SEND: ");
    for (int i = 0; i < m_size; i++) {
        printf_hex(buffer[i]);
        printf(" ");
    }
    printf("\n");
#endif
    self->send_buffer_descr[send_descriptor].m_avail = 0x8000;
    self->send_buffer_descr[send_descriptor].m_flags2 = 0;
    self->send_buffer_descr[send_descriptor].m_reserved = 0;
    uint32_t send_bs = (2048 / 256) << 16;
    self->send_buffer_descr[send_descriptor].m_flags = 0x80000000 | 0x03000000 | send_bs | ((uint16_t)((-m_size) & 0xFFF));
    jlos_io16_write(&self->m_register_address_port, 0);
    jlos_io16_write(&self->m_register_data_port, 0x48);

    {
        uint32_t sflags = 0;
        int sent = 0;
        for (volatile int delay = 0; delay < 500000; delay++) {
            __asm__("nop");
            sflags = self->send_buffer_descr[send_descriptor].m_flags;
            if ((sflags & 0x80000000) == 0) { sent = 1; break; }
        }
#if KERNEL_CONFIG_DEBUG_NETWORK
        jlos_io16_write(&self->m_register_address_port, 0);
        uint16_t csr0_send = jlos_io16_read(&self->m_register_data_port);
        printf("POST-SEND CSR0=0x");
        printf_hex((csr0_send >> 8) & 0xFF);
        printf_hex(csr0_send & 0xFF);
        printf(" RINT=");
        printf_hex((csr0_send & 0x0400) ? 1 : 0);
        printf(" TINT=");
        printf_hex((csr0_send & 0x0200) ? 1 : 0);
        printf(" INTR=");
        printf_hex((csr0_send & 0x0080) ? 1 : 0);
        printf(" INEA=");
        printf_hex((csr0_send & 0x0040) ? 1 : 0);
        printf(" RXON=");
        printf_hex((csr0_send & 0x0020) ? 1 : 0);
        printf(" TXON=");
        printf_hex((csr0_send & 0x0010) ? 1 : 0);
        printf(" SENT=");
        printf_hex(sent);
        printf(" SFLAGS=0x");
        printf_hex((sflags >> 24) & 0xFF);
        printf_hex((sflags >> 16) & 0xFF);
        printf_hex((sflags >> 8) & 0xFF);
        printf_hex(sflags & 0xFF);
        printf("\n");
#endif
    }
}

void jlos_amd_am79c973_receive(jlos_amd_am79c973_t* self)
{
    uint32_t bs = (2048 / 256) << 16;
    uint8_t start_idx = self->m_current_recv_buffer;
    int8_t last_processed_iter = -1;

    for (int i = 0; i < NUM_RECV_BUFFERS; i++) {
        int idx = (start_idx + i) % NUM_RECV_BUFFERS;
        uint32_t flags = self->recv_buffer_descr[idx].m_flags;
        uint32_t flags2 = self->recv_buffer_descr[idx].m_flags2;
        uint16_t avail = self->recv_buffer_descr[idx].m_avail;

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
            uint32_t m_size = flags2 & 0xFFF;
            if (m_size > 64) {
                m_size -= 4;
            }
            uint8_t *buffer = (uint8_t *)(self->recv_buffer_descr[idx].m_address);
            if (self->handler) {
                if (self->handler->on_raw_data_received(self->handler, buffer, m_size)) {
                    jlos_amd_am79c973_send(self, buffer, m_size);
                }
            }
        }
        self->recv_buffer_descr[idx].m_flags = (0x80000000 | bs | 0xF800);
        self->recv_buffer_descr[idx].m_flags2 = 0;
        self->recv_buffer_descr[idx].m_avail = 0x8000;
        self->recv_buffer_descr[idx].m_reserved = 0;
        last_processed_iter = i;
    }

    if (last_processed_iter >= 0) {
        self->m_current_recv_buffer = (start_idx + last_processed_iter + 1) % NUM_RECV_BUFFERS;
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
        mac |= ((uint64_t)self->m_init_block->physical_address[i]) << (8 * i);
    }
    return mac;
}

void jlos_amd_am79c973_set_ip_address(jlos_amd_am79c973_t* self, uint32_t ip)
{
    for (int i = 0; i < 4; i++) {
        self->m_init_block->m_logical_address[i] = (ip >> (8 * i)) & 0xFF;
    }
    for (int i = 4; i < 8; i++) {
        self->m_init_block->m_logical_address[i] = 0;
    }
}

uint32_t jlos_amd_am79c973_get_ip_address(jlos_amd_am79c973_t* self)
{
    uint32_t ip = 0;
    for (int i = 0; i < 4; i++) {
        ip |= ((uint32_t)self->m_init_block->m_logical_address[i]) << (8 * i);
    }
    return ip;
}