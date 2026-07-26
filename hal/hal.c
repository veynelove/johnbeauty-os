#include <hal/hal.h>
#include <hal/device.h>

/* hal.c 不依赖 kernel 头；直接 extern 用的输出函数 */
extern void printf(const char *str);
extern void printf_hex32(uint32_t value);
extern void printf_hex16(uint16_t value);
extern void printf_char(char c);

/* 前向声明：io ranges / irq table，后面定义，给前面的 sanity_check / claim 用 */
static jlos_hal_io_range_t  s_io_ranges[JLOS_HAL_IO_RANGES_MAX];
static jlos_hal_irq_info_t s_irq_table[JLOS_HAL_IRQ_MAX];

/* -------------------- 系统平台设备：由 jlos_hal_arch_init 注册 -------------------- */

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

/* -------------------- 硬件 Probe 结果：只读，jlos_hal_get_info() 导出 -------------------- */
static jlos_hal_info_t s_hal_info;

/* -------------------- x86 IO ops 两套实现 -------------------- */

static void hal_x86_fast_io8_init(jlos_io8_t *self, uint16_t port)
{ jlos_port8_bit_init(self, port); }
static void hal_x86_fast_io8_write(jlos_io8_t *self, uint8_t val)
{ jlos_port8_bit_write(self, val); }
static uint8_t hal_x86_fast_io8_read(jlos_io8_t *self)
{ return jlos_port8_bit_read(self); }

/* "fast" 下的 slow 变体：我们还是走 jlos_port8_bit_slow_* 底层，
 * 因为有些老设备（8259 PIC / PIT）无论 CPU 多快都要求 I/O 延迟。 */
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

/* slow 实现：所有 variant 都走 jlos_port*_slow_*（目前 port.c 只有 io8 有 slow 变体） */
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

/* 全局活跃 ops；默认 fast，jlos_hal_arch_init() 可按 CPUID 重设 */
const jlos_hal_io_ops_t *jlos_hal_io_ops = &jlos_hal_x86_fast_io_ops;

/* -------------------- IO sanity check -------------------- */

static int ranges_overlap(uint16_t a_s, uint16_t a_e, uint16_t b_s, uint16_t b_e)
{ return !(a_e < b_s || b_e < a_s); }

/* 小 helper：把 32-bit 无符号数以 10 进制打印（不依赖 sprintf） */
static void hal_print_u32_dec(uint32_t v)
{
    char buf[11];
    int i = 0;
    if (v == 0) { printf_char('0'); return; }
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i > 0) printf_char(buf[--i]);
}

int jlos_hal_io_sanity_check(uint16_t port, int is_write, const char *owner)
{
    (void)is_write;
    if (port > 0xFFFF) {
        printf("[HAL] io_sanity: port 0x");
        printf_hex16(port);
        printf(" out of range (owner=");
        printf(owner ? owner : "(null)");
        printf(")\n");
        return -1;
    }
    if ((port >= 0xCF8 && port <= 0xCFF)) {
        int found = 0;
        for (int i = 0; i < JLOS_HAL_IO_RANGES_MAX; i++) {
            if (s_io_ranges[i].claimed &&
                ranges_overlap(port, port, s_io_ranges[i].start, s_io_ranges[i].end)) {
                found = 1; break;
            }
        }
        if (!found) {
            printf("[HAL] io_sanity: WARN PCI config port 0x");
            printf_hex16(port);
            printf(" accessed without registered owner (caller=");
            printf(owner ? owner : "(null)");
            printf(")\n");
        }
    }
    return 0;
}

/* -------------------- IO ranges 注册去重 -------------------- */

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
            printf("[HAL] io_ranges: CONFLICT [0x");
            printf_hex16(start); printf("-0x"); printf_hex16(end);
            printf(" owner="); printf(owner ? owner : "(null)");
            printf("] overlaps existing [0x");
            printf_hex16(s_io_ranges[i].start); printf("-0x"); printf_hex16(s_io_ranges[i].end);
            printf(" owner="); printf(s_io_ranges[i].owner);
            printf("]\n");
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
    printf("[HAL] io_ranges: table full, cannot register [0x");
    printf_hex16(start); printf("-0x"); printf_hex16(end);
    printf("]\n");
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

/* -------------------- IRQ claim 保留机制 -------------------- */

int jlos_hal_irq_claim(uint8_t irq, const char *owner)
{
    if (s_irq_table[irq].claimed) {
        printf("[HAL] irq_claim: CONFLICT IRQ");
        hal_print_u32_dec(irq);
        printf(" already claimed by '");
        printf(s_irq_table[irq].owner);
        printf("', new requester=");
        printf(owner ? owner : "(null)");
        printf("\n");
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

/* -------------------- HAL 启动初始化 -------------------- */

void jlos_hal_arch_init(void)
{
    /* CPUID 探测未来在此处填 cpu_model / has_apic；目前默认现代 x86 */
    s_hal_info.cpu_model     = 0x00000686; /* 默认 686 类（QEMU 默认） */
    s_hal_info.cpu_has_cpuid = true;
    s_hal_info.cpu_has_apic  = true;

    /* 默认双 8259 级联，IRQ0 起始向量 0x20（与 irq_manager_init 第二个参数一致） */
    s_hal_info.irq_mode        = JLOS_HAL_IRQ_PIC_8259;
    s_hal_info.irq_base_vector = 0x20;

    /* 默认 8253 PIT，输入晶振 1193180 Hz */
    s_hal_info.timer_mode           = JLOS_HAL_TIMER_PIT_8253;
    s_hal_info.timer_input_clock_hz = 1193180ULL;

    /* PCI：目前只支持 Mechanism #1 (0xCF8/0xCFC)；ECAM 等 ACPI MCFG 解析后再填 */
    s_hal_info.pci_mmconfig_base   = 0;
    s_hal_info.pci_ecam_available  = false;

    /* 内存：预留典型 MMIO 区（未来从 E820 / ACPI 读真实值，覆盖这里）
     *  - 0x000A0000 ~ 0x000FFFFF：VGA / BIOS shadow / 预留 ROM
     *  - 0xFEE00000 ~ 0xFFFFFFFF：LAPIC / IOAPIC / PCI memory BAR / HPET 常用 */
    s_hal_info.mmio_reserved_start = 0x000A0000;
    s_hal_info.mmio_reserved_end   = 0xFFFFFFFF;

    /* CPUID 探测未来在此处添加；目前默认现代 x86，走 fast ops */
    jlos_hal_io_ops = &jlos_hal_x86_fast_io_ops;

    /* 系统平台设备统一注册：IO / IRQ / MMIO 资源在 device_register 内做冲突检查 */
    jlos_hal_device_register(&s_dev_pic_master);
    jlos_hal_device_register(&s_dev_pic_slave);
    jlos_hal_device_register(&s_dev_pit_timer);
    jlos_hal_device_register(&s_dev_pci_cfg);
    jlos_hal_device_register(&s_dev_uart_com1);

    /* jlos_hal_device_register 内部已调 jlos_hal_irq_claim → refresh_reserved_bitmap，
     * s_hal_info.irq_reserved_bitmap_31_0 自动同步 */
    jlos_hal_kernel_segments_init();
}

/* -------------------- 对外只读查询 + bitmap 同步 -------------------- */

const jlos_hal_info_t *jlos_hal_get_info(void)
{ return &s_hal_info; }

void jlos_hal_irq_refresh_reserved_bitmap(void)
{
    uint32_t bm = 0;
    /* 0-31：这是 x86 经典 IRQ 覆盖范围（PIC 0-15 + IOAPIC 扩展 16-31） */
    for (unsigned irq = 0; irq < 32u; irq++) {
        if (s_irq_table[irq].claimed) bm |= (1u << irq);
    }
    s_hal_info.irq_reserved_bitmap_31_0 = bm;
}
