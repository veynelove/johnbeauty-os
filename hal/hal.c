#include <hal/hal.h>
#include <hal/device.h>

#define JLOS_KERNEL_LOG_SUBSYS "hal"
#include <kernel/printk.h>

static jlos_hal_io_range_t s_io_ranges[JLOS_HAL_IO_RANGES_MAX];
static jlos_hal_irq_info_t s_irq_table[JLOS_HAL_IRQ_MAX];

static jlos_hal_info_t s_hal_info;

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

const jlos_hal_info_t *jlos_hal_get_info(void)
{ 
    return &s_hal_info;
}

jlos_hal_info_t *jlos_hal_info_get_for_init(void)
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
