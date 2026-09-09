#include <arch/x86/cpu_state.h>
#include <arch/x86/fpu_state.h>
#include <hal/hal.h>
#include <hal/spinlock.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>

static jlos_task_t      *s_fpu_owner = NULL;
static jlos_spinlock_t  s_fpu_lock = JLOS_SPINLOCK_INIT;
static bool             s_init_template_done = false;
static uint8_t          s_fxsave_template[512] __attribute__((aligned(16)));

extern jlos_task_t *g_current_task_ptr;

static void build_clean_template(void)
{
    if (s_init_template_done) {
        return;
    }
    __asm__ __volatile__("clts\n\t" ::: "memory");
    __asm__ __volatile__(
        "fninit\n\t"
        "ldmxcsr %0\n\t"
        "fxsave %1\n\t"
        :
        : "m"((const uint32_t){0x00001F80}), "m"(s_fxsave_template)
        : "memory"
    );
    s_init_template_done = true;
}

void jlos_arch_task_ext_init(jlos_task_t *task)
{
    build_clean_template();
    jlos_memcpy(task->ext_state.fxsave_area, s_fxsave_template, JLOS_ARCH_X86_FXSAVE_AREA_SIZE);
    task->ext_state.used = false;
}

void jlos_arch_task_ext_destroy(jlos_task_t *task)
{
    uint32_t fl = jlos_spin_lock_irqsave(&s_fpu_lock);
    if (s_fpu_owner == task) {
        s_fpu_owner = NULL;
    }
    jlos_spin_unlock_irqrestore(&s_fpu_lock, fl);
}

void jlos_arch_task_ext_switch(void)
{
    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "or $8, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"
        ::: "eax", "memory"
    );
}

void jlos_arch_task_ext_trap_body(void)
{
    __asm__ __volatile__("clts\n\t" ::: "memory");
    jlos_task_t *curr = g_current_task_ptr;
    if (!curr) {
        return;
    }
    uint32_t fl = jlos_spin_lock_irqsave(&s_fpu_lock);
    if (s_fpu_owner == curr) {
        goto clear_ts_and_out;
    }
    if (s_fpu_owner) {
        __asm__ __volatile__(
            "fxsave %0\n\t"
            : "=m"(s_fpu_owner->ext_state.fxsave_area)
            :
            : "memory"
        );
    }
    __asm__ __volatile__(
        "fxrstor %0\n\t"
        :
        : "m"(curr->ext_state.fxsave_area)
        : "memory"
    );
    s_fpu_owner = curr;
    curr->ext_state.used = true;

clear_ts_and_out:
    __asm__ __volatile__("clts\n\t" ::: "memory");
    jlos_spin_unlock_irqrestore(&s_fpu_lock, fl);
}
