#include <hal/spinlock.h>

void jlos_spinlock_init(jlos_spinlock_t *lock)
{
    if (!lock) return;
    lock->lock = 0;
    lock->irq_state = 0;
    lock->recursion_depth = 0;
}

void jlos_spinlock_destroy(jlos_spinlock_t *lock)
{
    if (!lock) return;
    lock->lock = 0;
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

    register volatile uint32_t * const lockp = &lock->lock;
    __asm__ __volatile__(
    "1:\n\t"
        "movl $1, %%eax\n\t"
        "xchgl %%eax, %1\n\t"
        "test %%eax, %%eax\n\t"
        "jz 2f\n\t"
        "pause\n\t"
        "jmp 1b\n\t"
    "2:\n\t"
        : "+m"(*lockp) : : "eax", "memory"
    );
    
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
        __asm__ __volatile__("movl $0, %0" : "+m"(lock->lock) :: "memory");
        __asm__ __volatile__("push %0; popf" :: "r"(flags) : "memory", "cc");
    }
}
