#include <hal/hal.h>
#include <hal/device.h>
#include <hal/kernel_syscall.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "hal"

static jlos_hal_io_range_t  s_io_ranges[JLOS_HAL_IO_RANGES_MAX];
static jlos_hal_irq_info_t s_irq_table[JLOS_HAL_IRQ_MAX];

static jlos_device_t s_dev_pic_master = {
    .name       = "8259 PIC Master",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 0xFF },
    .resources  = {
        { JLOS_RES_IO_PORT, 0x20, 0x21, 0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_pic_slave = {
    .name       = "8259 PIC Slave",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 2 },
    .resources  = {
        { JLOS_RES_IO_PORT, 0xA0, 0xA1, 0 },
        { JLOS_RES_IRQ,     2,    2,    0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_pit_timer = {
    .name       = "8253 PIT Timer",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 0 },
    .resources  = {
        { JLOS_RES_IO_PORT, 0x40, 0x43, 0 },
        { JLOS_RES_IRQ,     0,    0,    0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_pci_cfg = {
    .name       = "PCI Config Mechanism #1",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0, .mmio_size = 0, .irq = 0xFF },
    .resources  = {
        { JLOS_RES_IO_PORT, 0xCF8, 0xCFF, 0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_device_t s_dev_uart_com1 = {
    .name       = "16550 UART COM1",
    .bus_type   = JLOS_DEV_BUS_PLATFORM,
    .registered = false,
    .businfo.platform = { .mmio_base = 0x3F8, .mmio_size = 8, .irq = 4 },
    .resources  = {
        { JLOS_RES_IO_PORT, 0x3F8, 0x3FF, 0 },
        { 0 },
    },
    .drv = 0,
};

static jlos_hal_info_t s_hal_info;

static void hal_x86_fast_io8_init(jlos_io8_t *self, uint16_t port)
{ jlos_port8_bit_init(self, port); }
static void hal_x86_fast_io8_write(jlos_io8_t *self, uint8_t val)
{ jlos_port8_bit_write(self, val); }
static uint8_t hal_x86_fast_io8_read(jlos_io8_t *self)
{ return jlos_port8_bit_read(self); }

static void hal_x86_fast_io8_slow_init(jlos_io8_slow_t *self, uint16_t port)
{ jlos_port8_bit_slow_init(self, port); }
static void hal_x86_fast_io8_slow_write(jlos_io8_slow_t *self, uint8_t val)
{ jlos_port8_bit_slow_write(self, val); }
static uint8_t hal_x86_fast_io8_slow_read(jlos_io8_slow_t *self)
{ return jlos_port8_bit_slow_read(self); }

static void hal_x86_fast_io16_init(jlos_io16_t *self, uint16_t port)
{ jlos_port16_bit_init(self, port); }
static void hal_x86_fast_io16_write(jlos_io16_t *self, uint16_t val)
{ jlos_port16_bit_write(self, val); }
static uint16_t hal_x86_fast_io16_read(jlos_io16_t *self)
{ return jlos_port16_bit_read(self); }

static void hal_x86_fast_io32_init(jlos_io32_t *self, uint16_t port)
{ jlos_port32_bit_init(self, port); }
static void hal_x86_fast_io32_write(jlos_io32_t *self, uint32_t val)
{ jlos_port32_bit_write(self, val); }
static uint32_t hal_x86_fast_io32_read(jlos_io32_t *self)
{ return jlos_port32_bit_read(self); }

static void hal_x86_slow_io8_init(jlos_io8_t *self, uint16_t port)
{ jlos_port8_bit_slow_init((jlos_port8_bit_slow_t*)self, port); }
static void hal_x86_slow_io8_write(jlos_io8_t *self, uint8_t val)
{ jlos_port8_bit_slow_write((jlos_port8_bit_slow_t*)self, val); }
static uint8_t hal_x86_slow_io8_read(jlos_io8_t *self)
{ return jlos_port8_bit_slow_read((jlos_port8_bit_slow_t*)self); }

static void hal_x86_slow_io8_slow_init(jlos_io8_slow_t *self, uint16_t port)
{ jlos_port8_bit_slow_init(self, port); }
static void hal_x86_slow_io8_slow_write(jlos_io8_slow_t *self, uint8_t val)
{ jlos_port8_bit_slow_write(self, val); }
static uint8_t hal_x86_slow_io8_slow_read(jlos_io8_slow_t *self)
{ return jlos_port8_bit_slow_read(self); }

static void hal_x86_slow_io16_init(jlos_io16_t *self, uint16_t port)
{ jlos_port16_bit_init(self, port); }
static void hal_x86_slow_io16_write(jlos_io16_t *self, uint16_t val)
{ jlos_port16_bit_write(self, val); }
static uint16_t hal_x86_slow_io16_read(jlos_io16_t *self)
{ return jlos_port16_bit_read(self); }

static void hal_x86_slow_io32_init(jlos_io32_t *self, uint16_t port)
{ jlos_port32_bit_init(self, port); }
static void hal_x86_slow_io32_write(jlos_io32_t *self, uint32_t val)
{ jlos_port32_bit_write(self, val); }
static uint32_t hal_x86_slow_io32_read(jlos_io32_t *self)
{ return jlos_port32_bit_read(self); }

const jlos_hal_io_ops_t jlos_hal_x86_fast_io_ops = {
    .init_io8         = hal_x86_fast_io8_init,
    .write_io8        = hal_x86_fast_io8_write,
    .read_io8         = hal_x86_fast_io8_read,
    .init_io8_slow    = hal_x86_fast_io8_slow_init,
    .write_io8_slow   = hal_x86_fast_io8_slow_write,
    .read_io8_slow    = hal_x86_fast_io8_slow_read,
    .init_io16        = hal_x86_fast_io16_init,
    .write_io16       = hal_x86_fast_io16_write,
    .read_io16        = hal_x86_fast_io16_read,
    .init_io32        = hal_x86_fast_io32_init,
    .write_io32       = hal_x86_fast_io32_write,
    .read_io32        = hal_x86_fast_io32_read,
};

const jlos_hal_io_ops_t jlos_hal_x86_slow_io_ops = {
    .init_io8         = hal_x86_slow_io8_init,
    .write_io8        = hal_x86_slow_io8_write,
    .read_io8         = hal_x86_slow_io8_read,
    .init_io8_slow    = hal_x86_slow_io8_slow_init,
    .write_io8_slow   = hal_x86_slow_io8_slow_write,
    .read_io8_slow    = hal_x86_slow_io8_slow_read,
    .init_io16        = hal_x86_slow_io16_init,
    .write_io16       = hal_x86_slow_io16_write,
    .read_io16        = hal_x86_slow_io16_read,
    .init_io32        = hal_x86_slow_io32_init,
    .write_io32       = hal_x86_slow_io32_write,
    .read_io32        = hal_x86_slow_io32_read,
};

const jlos_hal_io_ops_t *jlos_hal_io_ops = &jlos_hal_x86_fast_io_ops;

static int ranges_overlap(uint16_t a_s, uint16_t a_e, uint16_t b_s, uint16_t b_e)
{ 
    return !(a_e < b_s || b_e < a_s);
}

int jlos_hal_io_sanity_check(uint16_t port, int is_write, const char *owner)
{
    (void)is_write;
    if ((port >= 0xCF8 && port <= 0xCFF)) {
        int found = 0;
        for (int i = 0; i < JLOS_HAL_IO_RANGES_MAX; i++) {
            if (s_io_ranges[i].claimed &&
                ranges_overlap(port, port, s_io_ranges[i].start, s_io_ranges[i].end)) {
                found = 1; break;
            }
        }
        if (!found) {
            printk_warn("io_sanity: pci config port 0x%x accessed without owner (caller=%s)\n",
                port, owner ? owner : "(null)");
        }
    }
    return 0;
}

static void strncpy_safe(char *dst, const char *src, int sz)
{
    if (!dst || !src || sz <= 0) return;
    int i;
    for (i = 0; i < sz - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

int jlos_hal_register_io_range(uint16_t start, uint16_t end, const char *owner)
{
    if (start > end) return -1;
    for (int i = 0; i < JLOS_HAL_IO_RANGES_MAX; i++) {
        if (s_io_ranges[i].claimed &&
            ranges_overlap(start, end, s_io_ranges[i].start, s_io_ranges[i].end)) {
            printk_warn("io_ranges: conflict [0x%x-0x%x owner=%s] overlaps [0x%x-0x%x owner=%s]\n",
                start, end, owner ? owner : "(null)",
                s_io_ranges[i].start, s_io_ranges[i].end, s_io_ranges[i].owner);
            return -1;
        }
    }
    for (int i = 0; i < JLOS_HAL_IO_RANGES_MAX; i++) {
        if (!s_io_ranges[i].claimed) {
            s_io_ranges[i].start = start;
            s_io_ranges[i].end   = end;
            s_io_ranges[i].claimed = 1;
            strncpy_safe(s_io_ranges[i].owner, owner ? owner : "(null)", JLOS_HAL_IO_OWNER_LEN);
            return 0;
        }
    }
    printk_err("io_ranges: table full, cannot register [0x%x-0x%x]\n", start, end);
    return -1;
}

int jlos_hal_unregister_io_range(uint16_t start, uint16_t end)
{
    for (int i = 0; i < JLOS_HAL_IO_RANGES_MAX; i++) {
        if (s_io_ranges[i].claimed &&
            s_io_ranges[i].start == start && s_io_ranges[i].end == end) {
            s_io_ranges[i].claimed = 0;
            s_io_ranges[i].owner[0] = '\0';
            return 0;
        }
    }
    return -1;
}

const jlos_hal_io_range_t *jlos_hal_get_io_ranges(int *out_count)
{
    if (out_count) *out_count = JLOS_HAL_IO_RANGES_MAX;
    return s_io_ranges;
}

int jlos_hal_irq_claim(uint8_t irq, const char *owner)
{
    if (s_irq_table[irq].claimed) {
        printk_warn("irq_claim: conflict irq%u already claimed by '%s', new requester=%s\n",
            irq, s_irq_table[irq].owner, owner ? owner : "(null)");
        return -1;
    }
    s_irq_table[irq].claimed = 1;
    strncpy_safe(s_irq_table[irq].owner, owner ? owner : "(null)", JLOS_HAL_IRQ_OWNER_LEN);
    jlos_hal_irq_refresh_reserved_bitmap();
    return 0;
}

int jlos_hal_irq_release(uint8_t irq)
{
    if (!s_irq_table[irq].claimed) return -1;
    s_irq_table[irq].claimed = 0;
    s_irq_table[irq].owner[0] = '\0';
    jlos_hal_irq_refresh_reserved_bitmap();
    return 0;
}

int jlos_hal_irq_is_claimed(uint8_t irq, char *out_owner, int owner_bufsz)
{
    if (!s_irq_table[irq].claimed) return 0;
    if (out_owner && owner_bufsz > 0) {
        strncpy_safe(out_owner, s_irq_table[irq].owner, owner_bufsz);
    }
    return 1;
}

const jlos_hal_irq_info_t *jlos_hal_get_irq_table(int *out_count)
{
    if (out_count) *out_count = JLOS_HAL_IRQ_MAX;
    return s_irq_table;
}

void jlos_hal_arch_init(void)
{
    s_hal_info.cpu_model     = 0x00000686; 
    s_hal_info.cpu_has_cpuid = true;
    s_hal_info.cpu_has_apic  = true;
    
    s_hal_info.irq_mode        = JLOS_HAL_IRQ_PIC_8259;
    s_hal_info.irq_base_vector = 0x20;
    
    s_hal_info.timer_mode           = JLOS_HAL_TIMER_PIT_8253;
    s_hal_info.timer_input_clock_hz = 1193180ULL;
    
    s_hal_info.pci_mmconfig_base   = 0;
    s_hal_info.pci_ecam_available  = false;

    s_hal_info.mmio_reserved_start = 0x000A0000;
    s_hal_info.mmio_reserved_end   = 0xFFFFFFFF;

    jlos_hal_io_ops = &jlos_hal_x86_fast_io_ops;
    
    jlos_hal_device_register(&s_dev_pic_master);
    jlos_hal_device_register(&s_dev_pic_slave);
    jlos_hal_device_register(&s_dev_pit_timer);
    jlos_hal_device_register(&s_dev_pci_cfg);
    jlos_hal_device_register(&s_dev_uart_com1);

    jlos_hal_arch_display_register();
    jlos_hal_kernel_segments_init();
    jlos_hal_arch_syscall_init();
}

const jlos_hal_info_t *jlos_hal_get_info(void)
{ 
    return &s_hal_info;
}

void jlos_hal_irq_refresh_reserved_bitmap(void)
{
    uint32_t bm = 0;
    
    for (unsigned irq = 0; irq < 32u; irq++) {
        if (s_irq_table[irq].claimed) bm |= (1u << irq);
    }
    s_hal_info.irq_reserved_bitmap_31_0 = bm;
}

jlos_hal_syscall_entry_fn          jlos_hal_syscall_entry          = 0;
jlos_hal_syscall_dispatch_fn       jlos_hal_syscall_dispatch       = 0;
jlos_hal_syscall_resched_check_fn  jlos_hal_syscall_resched_check  = 0;
jlos_hal_syscall_resched_do_fn     jlos_hal_syscall_resched_do     = 0;
