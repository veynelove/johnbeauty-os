#ifndef __JLOS_HAL_SPINLOCK_H
#define __JLOS_HAL_SPINLOCK_H

#include <tools/config.h>
#include <common/types.h>
#include <hal/barrier.h>

#define JLOS_SPINLOCK_INIT  { 0, 0, 0 }

typedef struct {
    volatile uint32_t lock;
    uint32_t irq_state;
    int recursion_depth;
} jlos_spinlock_t;

extern void jlos_spinlock_init(jlos_spinlock_t *lock);
extern void jlos_spinlock_destroy(jlos_spinlock_t *lock);
extern uint32_t jlos_spin_lock_irqsave(jlos_spinlock_t *lock);
extern void jlos_spin_unlock_irqrestore(jlos_spinlock_t *lock, uint32_t flags);

#endif
