#include <hal/timer.h>
#include <hal/io.h>
#include <kernel/initcall.h>

#define JLOS_PIT_INPUT_HZ     1193180U
#define JLOS_PIT_CMD_PORT     0x43
#define JLOS_PIT_CH0_PORT     0x40
#define JLOS_PIT_CMD_MODE3    0x36

void pit_start_periodic(uint16_t freq_hz)
{
    jlos_io8_slow_t pit_cmd, pit_ch0;
    jlos_io8_slow_init(&pit_cmd, JLOS_PIT_CMD_PORT);
    jlos_io8_slow_init(&pit_ch0, JLOS_PIT_CH0_PORT);

    uint16_t divisor;
    if (freq_hz == 0) {
        divisor = 0;
    } else {
        divisor = (uint16_t)(JLOS_PIT_INPUT_HZ / (uint32_t)freq_hz);
    }

    jlos_io8_slow_write(&pit_cmd, JLOS_PIT_CMD_MODE3);
    jlos_io8_slow_write(&pit_ch0, (uint8_t)(divisor & 0xFF));
    jlos_io8_slow_write(&pit_ch0, (uint8_t)((divisor >> 8) & 0xFF));
}

static jlos_timer_device_t s_pit_timer_dev = {
    .name           = "8253 PIT",
    .rating         = 100,
    .start_periodic = pit_start_periodic,
};

static void pit_register(void)
{
    jlos_hal_timer_register(&s_pit_timer_dev);
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, pit_register);
