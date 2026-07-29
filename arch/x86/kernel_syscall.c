#include <arch/x86/kernel_syscall.h>
#include <arch/x86/interrupts.h>
#include <kernel/syscall.h>
#include <kernel/multitask.h>

extern void printf(const char *str);
extern jlos_task_manager_t *g_task_manager_ptr;
extern jlos_task_t *g_current_task_ptr;

static uint32_t syscall_write(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    char buf[256];
    uint32_t len = arg2;
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    if (!jlos_copy_from_user(buf, (void *)arg1, len)) {
        return -1;
    }
    buf[len] = '\0';
    printf(buf);
    return len;
}

static uint32_t syscall_exit(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (g_current_task_ptr) {
        g_current_task_ptr->m_status = JLOS_TASK_TERMINATED;
    }
    return 0;
}

static uint32_t syscall_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (g_current_task_ptr) {
        jlos_task_manager_schedule(g_task_manager_ptr, NULL);
    }
    return 0;
}

void jlos_syscall_handler_init(jlos_syscall_handler_t* self, jlos_interrupt_manager_t *interrupt_manager, uint8_t m_interrupt_number)
{
    self->m_interrupt_manager = interrupt_manager;
    self->m_interrupt_number = m_interrupt_number;
    jlos_interrupt_handler_init((jlos_interrupt_handler_t*)self,
        interrupt_manager, m_interrupt_number);
    self->handle_interrupt = jlos_syscall_handler_handle_interrupt;
    for (int i = 0; i < JLOS_SYSCALL_MAX; i++) {
        self->m_dispatch[i] = 0;
    }
    self->m_dispatch[JLOS_SYSCALL_WRITE] = syscall_write;
    self->m_dispatch[JLOS_SYSCALL_EXIT] = syscall_exit;
    self->m_dispatch[JLOS_SYSCALL_YIELD] = syscall_yield;
}

void jlos_syscall_handler_destroy(jlos_syscall_handler_t* self)
{
}

uint32_t jlos_syscall_handler_handle_interrupt(jlos_syscall_handler_t* self, uint32_t m_esp)
{
    jlos_cpu_state_t *cpu = (jlos_cpu_state_t *)m_esp;
    uint32_t syscall_num = cpu->m_eax;
    if (syscall_num < JLOS_SYSCALL_MAX && self->m_dispatch[syscall_num]) {
        cpu->m_eax = self->m_dispatch[syscall_num](cpu->m_ebx, cpu->m_ecx, cpu->m_edx);
    } else {
        cpu->m_eax = -1;
    }
    /* syscall_exit 标记任务 TERMINATED 后不能返回 ring3，需立即切换到下一个任务 */
    if (g_current_task_ptr && g_current_task_ptr->m_status == JLOS_TASK_TERMINATED) {
        return (uint32_t)jlos_task_manager_schedule(g_task_manager_ptr, cpu);
    }
    return m_esp;
}
