#include <hal/clocksource.h>
#include <hal/io.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "tsc"
#include <kernel/printk.h>

#define JLOS_TSC_SHIFT          22
#define JLOS_PIT_INPUT_HZ       1193180
#define JLOS_PIT_CH2_PORT       0x42
#define JLOS_PIT_CMD_PORT       0x43
#define JLOS_PIT_CH2_GATE_PORT  0x61
#define JLOS_PIT_CALIB_DIVISOR  11932

static jlos_clocksource_t s_tsc_cs;

static uint64_t tsc_read(void)
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static uint64_t tsc_calibrate(void)
{
    jlos_io8_slow_t gate_port, ch2_port, cmd_port;
    jlos_io8_slow_init(&gate_port, JLOS_PIT_CH2_GATE_PORT);
    jlos_io8_slow_init(&ch2_port, JLOS_PIT_CH2_PORT);
    jlos_io8_slow_init(&cmd_port, JLOS_PIT_CMD_PORT);

    uint8_t gate_orig = jlos_io8_slow_read(&gate_port);

    jlos_io8_slow_write(&gate_port, gate_orig & ~0x03);

    jlos_io8_slow_write(&cmd_port, 0xB0);

    jlos_io8_slow_write(&ch2_port, (uint8_t)(JLOS_PIT_CALIB_DIVISOR & 0xFF));
    jlos_io8_slow_write(&ch2_port, (uint8_t)((JLOS_PIT_CALIB_DIVISOR >> 8) & 0xFF));

    uint64_t start = tsc_read();

    jlos_io8_slow_write(&gate_port, (gate_orig & ~0x02) | 0x01);

    while ((jlos_io8_slow_read(&gate_port) & 0x20) == 0) {
    }

    uint64_t end = tsc_read();

    jlos_io8_slow_write(&gate_port, gate_orig);

    return (end - start) * JLOS_PIT_INPUT_HZ / JLOS_PIT_CALIB_DIVISOR;
}

static void tsc_register(void)
{
    uint64_t freq = tsc_calibrate();
    if (freq == 0) {
        printk_err("calibration failed\n");
        return;
    }

    s_tsc_cs.name = "TSC";
    s_tsc_cs.rating = 300;
    s_tsc_cs.read = tsc_read;
    s_tsc_cs.shift = JLOS_TSC_SHIFT;
    s_tsc_cs.mult = (uint32_t)((1000000000ULL << JLOS_TSC_SHIFT) / freq);
    s_tsc_cs.mask = 0xFFFFFFFFFFFFFFFFULL;

    jlos_clocksource_register(&s_tsc_cs);
    printk_info("freq = %u MHz\n", (uint32_t)(freq / 1000000));
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, tsc_register);
