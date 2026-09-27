#include <hal/clock_event.h>
#include <hal/io.h>
#include <kernel/initcall.h>

#define JLOS_PIT_INPUT_HZ     1193180U
#define JLOS_PIT_CMD_PORT     0x43
#define JLOS_PIT_CH0_PORT     0x40
#define JLOS_PIT_CMD_MODE3    0x36

static void pit_set_state_periodic(uint32_t hz)
{
    jlos_io8_slow_t pit_cmd, pit_ch0;
    jlos_io8_slow_init(&pit_cmd, JLOS_PIT_CMD_PORT);
    jlos_io8_slow_init(&pit_ch0, JLOS_PIT_CH0_PORT);

    uint16_t divisor;
    if (hz == 0) {
        divisor = 0;
    } else {
        divisor = (uint16_t)(JLOS_PIT_INPUT_HZ / hz);
    }

    jlos_io8_slow_write(&pit_cmd, JLOS_PIT_CMD_MODE3);
    jlos_io8_slow_write(&pit_ch0, (uint8_t)(divisor & 0xFF));
    jlos_io8_slow_write(&pit_ch0, (uint8_t)((divisor >> 8) & 0xFF));
}

static void pit_set_state_shutdown(void)
{
    jlos_io8_slow_t pit_cmd;
    jlos_io8_slow_init(&pit_cmd, JLOS_PIT_CMD_PORT);
    jlos_io8_slow_write(&pit_cmd, 0x30);
}

static jlos_clock_event_device_t s_pit_evt_dev = {
    .name                = "8253 PIT",
    .rating              = 100,
    .features            = JLOS_CLOCK_EVT_FEAT_PERIODIC,
    .set_state_periodic  = pit_set_state_periodic,
    .set_state_shutdown  = pit_set_state_shutdown,
};

static void pit_register(void)
{
    jlos_clock_event_register(&s_pit_evt_dev);
}

JLOS_INITCALL(JLOS_INITCALL_SUBSYS, pit_register);
