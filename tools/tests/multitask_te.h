#ifndef JLOS_MULTI_TASK_TEST_H
#define JLOS_MULTI_TASK_TEST_H

#include <kernel/gdt.h>
#include <kernel/multitask.h>

void multitask_test(jlos_gdt_t *gdt, jlos_task_manager_t *task_manager_);

#endif
