/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hal/spinlock.h>

static inline uint16_t jlos_arm64_fetch_add16(volatile uint16_t *p, uint16_t val)
{
    uint16_t old, tmp;
    __asm__ __volatile__(
        "1: ldaxrh %w[old], [%[p]]\n"
        "   add %w[old], %w[old], %w[val]\n"
        "   stxrh %w[tmp], %w[old], [%[p]]\n"
        "   cbnz %w[tmp], 1b\n"
        : [old] "=&r"(old), [tmp] "=&r"(tmp)
        : [p] "r"(p), [val] "r"(val)
        : "memory");
    return old - val;
}

static inline uint16_t jlos_arm64_load_acquire16(volatile uint16_t *p)
{
    uint16_t v;
    __asm__ __volatile__(
        "ldarh %w[v], [%[p]]"
        : [v] "=r"(v)
        : [p] "r"(p)
        : "memory");
    return v;
}

static inline void jlos_arm64_store_release16(volatile uint16_t *p, uint16_t val)
{
    __asm__ __volatile__(
        "stlrh %w[v], [%[p]]"
        :: [p] "r"(p), [v] "r"(val)
        : "memory");
}

void jlos_spinlock_init(jlos_spinlock_t *lock)
{
    if (!lock) {
        return;
    }
    lock->lock_word.value = 0;
}

void jlos_spinlock_destroy(jlos_spinlock_t *lock)
{
    if (!lock) {
        return;
    }
    lock->lock_word.value = 0;
}

void jlos_spin_lock(jlos_spinlock_t *lock)
{
    if (!lock) {
        return;
    }
    uint16_t my_ticket = jlos_arm64_fetch_add16(&lock->lock_word.tickets.next, 1);
    while (jlos_arm64_load_acquire16(&lock->lock_word.tickets.owner) != my_ticket) {
        __asm__ __volatile__("yield" ::: "memory");
    }
    jlos_mb();
}

void jlos_spin_unlock(jlos_spinlock_t *lock)
{
    if (!lock) {
        return;
    }
    jlos_mb();
    uint16_t next_owner = lock->lock_word.tickets.owner + 1;
    jlos_arm64_store_release16(&lock->lock_word.tickets.owner, next_owner);
}

uint32_t jlos_spin_lock_irqsave(jlos_spinlock_t *lock)
{
    if (!lock) {
        return 0;
    }
    uint32_t daif;
    __asm__ __volatile__("mrs %0, daif" : "=r"(daif));
    __asm__ __volatile__("msr daifset, #2" ::: "memory");
    jlos_spin_lock(lock);
    return daif;
}

void jlos_spin_unlock_irqrestore(jlos_spinlock_t *lock, uint32_t flags)
{
    if (!lock) {
        return;
    }
    jlos_spin_unlock(lock);
    __asm__ __volatile__("msr daif, %0" :: "r"(flags) : "memory");
}
