#include <hal/spinlock.h>

void jlos_spinlock_init(jlos_spinlock_t *lock)
{
    if (!lock) return;
    lock->lock_word.value = 0;
    lock->irq_state = 0;
    lock->recursion_depth = 0;
}

void jlos_spinlock_destroy(jlos_spinlock_t *lock)
{
    if (!lock) return;
    lock->lock_word.value = 0;
    lock->irq_state = 0;
    lock->recursion_depth = 0;
}

uint32_t jlos_spin_lock_irqsave(jlos_spinlock_t *lock)
{
    if (!lock) return 0;
    uint32_t flags;
    __asm__ __volatile__("pushf; pop %0; cli" : "=r"(flags) :: "memory");

    if (lock->recursion_depth > 0) {
        lock->recursion_depth++;
        return flags;
    }

    uint16_t my_ticket;
    __asm__ __volatile__(
        "movw $1, %%ax\n\t"
        "lock xaddw %%ax, %1\n\t"
        : "=a"(my_ticket), "+m"(lock->lock_word.tickets.next)
        : : "memory"
    );
    while (lock->lock_word.tickets.owner != my_ticket) {
        __asm__ __volatile__("pause" ::: "memory");
    }
    
    jlos_mb();
    lock->recursion_depth = 1;
    lock->irq_state = flags;
    return flags;
}

void jlos_spin_unlock_irqrestore(jlos_spinlock_t *lock, uint32_t flags)
{
    if (!lock || lock->recursion_depth <= 0) return;
    lock->recursion_depth--;
    if (lock->recursion_depth == 0) {
        jlos_mb();
        lock->lock_word.tickets.owner++;
        __asm__ __volatile__("push %0; popf" :: "r"(flags) : "memory", "cc");
    }
}
