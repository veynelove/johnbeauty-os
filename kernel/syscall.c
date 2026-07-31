#include <kernel/syscall.h>
#include <kernel/paging.h>
#include <kernel/memory_manager.h>
#include <kernel/printk.h>
#include <hal/timer.h>
#include <hal/kernel_syscall.h>

extern jlos_task_manager_t *g_task_manager_ptr;
extern jlos_task_t *g_current_task_ptr;

jlos_syscall_handler_t *s_syscall_handler = NULL;

static int32_t syscall_write(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t len = arg2;
    if (len > 4096) {
        len = 4096;
    }
    uint8_t *buf = (uint8_t *)jlos_malloc(len + 1);
    if (!buf) {
        return -SYSCALL_ENOMEM;
    }
    if (!jlos_copy_from_user(buf, (void *)arg1, len)) {
        jlos_free(buf);
        return -SYSCALL_EFAULT;
    }
    buf[len] = '\0';
    printf((const char *)buf);
    jlos_free(buf);
    return len;
}

static int32_t syscall_exit(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENULL;
    }
    g_current_task_ptr->m_status = JLOS_TASK_TERMINATED;
    g_current_task_ptr->m_exit_code = arg1;
    return 0;
}

static int32_t syscall_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENULL;
    }
    g_current_task_ptr->m_yield = true;
    return 0;
}

static int32_t syscall_get_pid(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENULL;
    }
    return g_current_task_ptr->m_pid;
}

static int32_t syscall_sleep(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENULL;
    }
    g_current_task_ptr->m_wake_tick = jlos_hal_timer_get_ticks() + arg1;
    g_current_task_ptr->m_sleeping = true;
    return 0;
}

static int32_t syscall_get_errno(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENULL;
    }
    return g_current_task_ptr->m_errno;
}

static int32_t syscall_get_ticks(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    return (int32_t)jlos_hal_timer_get_ticks();
}

static int32_t syscall_debug_tasks(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_task_manager_ptr) {
        return - SYSCALL_ENULL;
    }
    printf("=== task list ===\n");
    for (int i = 0; i < g_task_manager_ptr->m_num_tasks; i++) {
        jlos_task_t *t = g_task_manager_ptr->tasks[i];
        if (t) {
            printk("[%d] name = %s, pid = %u, status = %d, task_type = %s\n", i, t->m_name,
                t->m_pid, t->m_status, t->m_is_user_process ? "user" : "kernel");
        }
    }
    printf("================\n");
    return 0;
}

static int32_t syscall_wait_pid(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t target_pid = arg1;
    int32_t *exit_code = (int32_t *)arg2;

    if (!g_current_task_ptr || !g_task_manager_ptr) {
        return -SYSCALL_ENULL;
    }
    jlos_task_t *target = NULL;
    for (int i = 0; i < g_task_manager_ptr->m_num_tasks; i++) {
        jlos_task_t *t = g_task_manager_ptr->tasks[i];
        if (t && t->m_pid == target_pid && t->m_parent_pid == g_current_task_ptr->m_pid) {
            target = t;
            break;
        }
    }
    if (!target) {
        return -SYSCALL_ENINVAL;
    }
    if (target->m_status == JLOS_TASK_TERMINATED) {
        if (exit_code) {
            if (!jlos_copy_to_user(exit_code, &target->m_exit_code, sizeof(int32_t))) {
                return -SYSCALL_EFAULT;
            }
        }
        return (int32_t)target->m_pid;
    }
    g_current_task_ptr->m_waiting_pid = target_pid;
    g_current_task_ptr->m_status = JLOS_TASK_WAITING;
    return 0;
}

void jlos_syscall_register(uint8_t num, jlos_syscall_func_t handler)
{
    if (!s_syscall_handler) {
        return;
    }
    s_syscall_handler->m_dispatch[num] = handler;
}

void jlos_syscall_handler_init(jlos_syscall_handler_t* self, jlos_irq_manager_t *interrupt_manager, uint8_t m_interrupt_number)
{
    s_syscall_handler = self;
    self->m_interrupt_manager = interrupt_manager;
    self->m_interrupt_number = m_interrupt_number;
    jlos_irq_handler_init((jlos_irq_handler_t *)self, interrupt_manager, m_interrupt_number);
    self->handle_interrupt = jlos_syscall_handler_handle_interrupt;
    for (int i = 0; i < JLOS_SYSCALL_MAX; i++) {
        self->m_dispatch[i] = 0;
    }
    jlos_syscall_register(JLOS_SYSCALL_WRITE, syscall_write);
    jlos_syscall_register(JLOS_SYSCALL_YIELD, syscall_yield);
    jlos_syscall_register(JLOS_SYSCALL_EXIT, syscall_exit);
    jlos_syscall_register(JLOS_SYSCALL_GET_PID, syscall_get_pid);
    jlos_syscall_register(JLOS_SYSCALL_SLEEP, syscall_sleep);
    jlos_syscall_register(JLOS_SYSCALL_GET_ERRNO, syscall_get_errno);
    jlos_syscall_register(JLOS_SYSCALL_GET_TICKS, syscall_get_ticks);
    jlos_syscall_register(JLOS_SYSCALL_DEBUG_TASKS, syscall_debug_tasks);
    jlos_syscall_register(JLOS_SYSCALL_WAIT_PID, syscall_wait_pid);
}

void jlos_syscall_handler_destroy(jlos_syscall_handler_t* self)
{
}

int32_t jlos_syscall_do_dispatch(jlos_syscall_handler_t *self, uint32_t syscall_num, uint32_t arg1, uint32_t arg2,
    uint32_t arg3)
{
    int32_t result = - SYSCALL_ENOSYS;
    if (syscall_num < JLOS_SYSCALL_MAX && self->m_dispatch[syscall_num]) {
        result = self->m_dispatch[syscall_num](arg1, arg2, arg3);
    }
    if (g_current_task_ptr) {
        g_current_task_ptr->m_errno = (result < 0) ? -result : 0;
        return (result < 0) ? -1 : result;
    }
    return result;
}

bool jlos_syscall_need_resched()
{
    if (!g_current_task_ptr) {
        return false;
    }
    if (g_current_task_ptr->m_status == JLOS_TASK_TERMINATED || g_current_task_ptr->m_status == JLOS_TASK_WAITING
    || g_current_task_ptr->m_sleeping || g_current_task_ptr->m_yield) {
        return true;
    }
    return false;
}

bool jlos_access_ok(const void *addr, size_t n)
{
    return jlos_paging_is_user_accessible((uint32_t)addr, n);
}

bool jlos_copy_from_user(void *dst, const void *usr_src, size_t n)
{
    if (!jlos_access_ok(usr_src, n)) {
        return false;
    }
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)usr_src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return true;
}

bool jlos_copy_to_user(void *usr_dst, const void *ker_src, size_t n)
{
    if (!jlos_access_ok(usr_dst, n)) {
        return false;
    }
    uint8_t *d = (uint8_t *)usr_dst;
    const uint8_t *s = (const uint8_t *)ker_src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return true;
}
