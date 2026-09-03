#ifndef _JLOS_HAL_SYMMETRIC_MULTI_PROCESSING_H
#define _JLOS_HAL_SYMMETRIC_MULTI_PROCESSING_H

#include <common/types.h>

#define JLOS_MAX_CPUS 1
#define JLOS_CACHELINE_SIZE 64

uint32_t jlos_hal_get_cpu_id(void);
uint32_t jlos_hal_num_cpus(void);

#endif
