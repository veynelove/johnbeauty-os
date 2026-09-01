#include <hal/smp.h>

uint32_t jlos_hal_get_cpu_id(void)
{
    return 0;
}

uint32_t jlos_hal_num_cpus(void)
{
    return JLOS_MAX_CPUS;
}