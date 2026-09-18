#include <hal/timer.h>
#include <kernel/initcall.h>
#include <kernel/printk.h>

#define JLOS_KERNEL_LOG_SUBSYS "timer"

static volatile uint32_t s_hal_timer_ticks;
static JLOS_LIST_HEAD(s_timer_devs);
static jlos_timer_device_t *s_active_timer;

void jlos_hal_timer_register(jlos_timer_device_t *dev)
{
    if (!dev) {
        return;
    }
    jlos_list_add(&dev->list, &s_timer_devs);
}

jlos_timer_device_t *jlos_hal_timer_get_active(void)
{
    return s_active_timer;
}

void jlos_hal_timer_start_periodic(uint16_t freq_hz)
{
    if (s_active_timer && s_active_timer->start_periodic) {
        s_active_timer->start_periodic(freq_hz);
    }
    s_hal_timer_ticks = 0;
}

uint32_t jlos_hal_timer_get_ticks(void)
{
    return s_hal_timer_ticks;
}

void jlos_hal_timer_reset_ticks(void)
{ 
    s_hal_timer_ticks = 0;
}

void jlos_hal_timer_on_tick(void)
{ 
    s_hal_timer_ticks++;
}

static void timer_select_and_start(void)
{
    jlos_timer_device_t *best = NULL;
    jlos_timer_device_t *d;
    jlos_list_for_each_entry(d, &s_timer_devs, list) {
        if (!best || d->rating > best->rating) {
            best = d;
        }
    }
    s_active_timer = best;
    if (best) {
        printk_info("selected = %s, rating = %u\n", best->name, best->rating);
    } else {
        printk_err("no device registered\n");
        return;
    }
    jlos_hal_timer_start_periodic(JLOS_HAL_TIME_FREQ_HZ);
}

JLOS_INITCALL(JLOS_INITCALL_DEVICE, timer_select_and_start);
