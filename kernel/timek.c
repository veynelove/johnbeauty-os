#include <kernel/timek.h>
#include <hal/clocksource.h>
#include <hal/rtc.h>
#include <kernel/initcall.h>

#define JLOS_KERNEL_LOG_SUBSYS "timek"
#include <kernel/printk.h>

static uint64_t             s_realtime_base_ns;
static uint64_t             s_last_cycles;
static uint64_t             s_monotonic_ns;
static bool                 s_timek_inited;
static volatile uint32_t    s_ticks;

void jlos_timek_init(void)
{
    if (s_timek_inited) {
        return;
    }
    jlos_clocksource_t *cs = jlos_clocksource_get_active();
    if (!cs) {
        return;
    }
    s_last_cycles = cs->read();
    s_monotonic_ns = 0;
    s_realtime_base_ns = 0;
    s_timek_inited = true;
    s_ticks = 0;
}

uint64_t jlos_timek_get_monotonic_ns(void)
{
    if (!s_timek_inited) {
        jlos_timek_init();
    }
    jlos_clocksource_t *cs = jlos_clocksource_get_active();
    if (!cs || !cs->read) {
        return s_monotonic_ns;
    }
    uint64_t cycles = cs->read();
    uint64_t delta = (cycles - s_last_cycles) & cs->mask;
    s_last_cycles = cycles;
    s_monotonic_ns += jlos_clocksource_cycles_to_ns(delta);
    return s_monotonic_ns;
}

uint64_t jlos_timek_get_realtime_ns(void)
{
    return s_realtime_base_ns + jlos_timek_get_monotonic_ns();
}

void jlos_timek_set_realtime(uint64_t ns)
{
    s_realtime_base_ns = ns - jlos_timek_get_monotonic_ns();
}

void jlos_timek_on_tick(void)
{
    if (!s_timek_inited) {
        jlos_timek_init();
    }
    s_ticks++;
    jlos_timek_get_monotonic_ns();
}

uint32_t jlos_timek_get_ticks(void)
{
    return s_ticks;
}

void jlos_timek_reset_ticks(void)
{
    s_ticks = 0;
}

static void jlos_timek_rtc_init(void)
{
    jlos_timek_set_realtime(jlos_hal_rtc_read_ns());
}

JLOS_INITCALL(JLOS_INITCALL_LATE, jlos_timek_rtc_init);
