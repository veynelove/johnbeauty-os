#include <kernel/syscall.h>
#include <kernel/paging.h>
#include <kernel/memory_manager.h>
#include <kernel/ipc.h>
#include <kernel/printk.h>
#include <kernel/page_frame_allocator.h>
#include <hal/timer.h>
#include <hal/kernel_syscall.h>

extern jlos_task_manager_t *g_task_manager_ptr;
extern jlos_task_t *g_current_task_ptr;

jlos_syscall_handler_t *s_syscall_handler = NULL;

static int32_t syscall_write(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    int32_t fd = (int32_t)arg1;
    void *user_buf = (void *)arg2;
    uint32_t len = arg3;
    if (len > JLOS_SYSCALL_WRITE_BUF_SIZE_MAX) {
        len = JLOS_SYSCALL_WRITE_BUF_SIZE_MAX;
    }
    if (!g_current_task_ptr || !g_current_task_ptr->fds) {
        return -SYSCALL_ENOMEM;
    }
    jlos_task_fd_t *fd_entry = jlos_task_fd_get(g_current_task_ptr, fd);
    if (!fd_entry) {
        return - SYSCALL_ENINVAL;
    }
    if (fd_entry->type == JLOS_TASK_FD_CONSOLE && len < JLOS_SYSCALL_WRITE_BUF_SIZE_MAX) {
        len += 1;
    }
    uint8_t stack_buf[256];
    uint8_t *buf = NULL;
    uint32_t alloc_len = len;
    if (alloc_len <= sizeof(stack_buf)) {
        buf = stack_buf;
    } else {
        buf = (uint8_t *)jlos_malloc(len);
        if (!buf) {
            return -SYSCALL_ENOMEM;
        }
    }
    if (!jlos_copy_from_user(buf, user_buf, len)) {
        if (buf != stack_buf) {
            jlos_free(buf);
        }
        return -SYSCALL_EFAULT;
    }
    if (fd_entry->type == JLOS_TASK_FD_CONSOLE) {
        buf[len - 1] = '\0';
        printk("%s", (const char *)buf);
        if (buf != stack_buf) {
            jlos_free(buf);
        }
        return len;
    }
    if (fd_entry->type == JLOS_TASK_FD_PIPE) {
        jlos_pipe_t *pipe = (jlos_pipe_t *)fd_entry->obj;
        uint32_t written = jlos_pipe_write(pipe, buf, len);
        if (buf != stack_buf) {
            jlos_free(buf);
        }
        return written;
    }
    if (buf != stack_buf) {
        jlos_free(buf);
    }
    return -SYSCALL_ENINVAL;
}

static int32_t syscall_read(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    int32_t fd = (int32_t)arg1;
    void *user_buf = (void *)arg2;
    uint32_t len = arg3;
    if (!g_current_task_ptr || !g_current_task_ptr->fds) {
        return -SYSCALL_ENOMEM;
    }
    jlos_task_fd_t *fd_entry = jlos_task_fd_get(g_current_task_ptr, fd);
    if (!fd_entry) {
        return -SYSCALL_ENOMEM;
    }
    if (fd_entry->type == JLOS_TASK_FD_PIPE) {
        jlos_pipe_t *pipe = (jlos_pipe_t *)fd_entry->obj;
        uint8_t stack_buf[256];
        uint8_t *buf = NULL;
        if (len <= sizeof(stack_buf)) {
            buf = stack_buf;
        } else {
            buf = (uint8_t *)jlos_malloc(len);
            if (!buf) {
                return -SYSCALL_ENOMEM;
            }
        }
        uint32_t read = jlos_pipe_read(pipe, buf, len);
        if (!jlos_copy_to_user(user_buf, buf, read)) {
            if (buf != stack_buf) {
                jlos_free(buf);
            }
            return -SYSCALL_EFAULT;
        }
        if (buf != stack_buf) {
            jlos_free(buf);
        }
        return read;
    }
    return -SYSCALL_ENINVAL;
}

static int32_t syscall_create_pipe(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr || !g_current_task_ptr->fds) {
        return -SYSCALL_ENOMEM;
    }
    jlos_pipe_t *pipe = (jlos_pipe_t *)jlos_malloc(sizeof(jlos_pipe_t));
    if (!pipe) {
        return -SYSCALL_ENOMEM;
    }
    jlos_pipe_init(pipe, 0);
    int32_t fd_read = jlos_task_fd_malloc(g_current_task_ptr);
    if (fd_read < 0) {
        jlos_pipe_destroy(pipe);
        return -SYSCALL_ENOMEM;
    }
    int32_t fd_write = jlos_task_fd_malloc(g_current_task_ptr);
    if (fd_write < 0) {
        jlos_task_fd_free(g_current_task_ptr, fd_read);
        jlos_pipe_destroy(pipe);
        return -SYSCALL_ENOMEM;
    }
    jlos_task_fd_t *fd_r = jlos_task_fd_get(g_current_task_ptr, fd_read);
    fd_r->type = JLOS_TASK_FD_PIPE;
    fd_r->obj = pipe;
    fd_r->flags = JLOS_TASK_FD_READ_ONLY;
    jlos_task_fd_t *fd_w = jlos_task_fd_get(g_current_task_ptr, fd_write);
    fd_w->type = JLOS_TASK_FD_PIPE;
    fd_w->obj = pipe;
    fd_w->flags = JLOS_TASK_FD_WRITE_ONLY;
    pipe->refcount++;
    int32_t fds[2] = {fd_read, fd_write};
    if (!jlos_copy_to_user((void *)arg1, fds, sizeof(fds))) {
        jlos_task_fd_free(g_current_task_ptr, fd_read);
        jlos_task_fd_free(g_current_task_ptr, fd_write);
        jlos_pipe_destroy(pipe);
        return -SYSCALL_EFAULT;
    }
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_task_fd_close(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    int32_t fd = (int32_t)arg1;
    if (!g_current_task_ptr || !g_current_task_ptr->fds) {
        return -SYSCALL_ENOMEM;
    }
    jlos_task_fd_t *fd_entry = jlos_task_fd_get(g_current_task_ptr, fd);
    if (!fd_entry) {
        return -SYSCALL_ENINVAL;
    }
    if (fd_entry->type == JLOS_TASK_FD_PIPE) {
        jlos_pipe_t *pipe = (jlos_pipe_t *)fd_entry->obj;
        if (pipe) {
            jlos_pipe_close(pipe);
            if (--pipe->refcount == 0) {
                jlos_pipe_destroy(pipe);
            }
        }
    }
    jlos_task_fd_free(g_current_task_ptr, fd);
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_task_brk(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    if (arg1 == 0) {
        return (int32_t)g_current_task_ptr->brk_end;
    }
    uint32_t new_brk = arg1;
    if (new_brk < g_current_task_ptr->brk_start || new_brk > g_current_task_ptr->brk_limit) {
        return (int32_t)g_current_task_ptr->brk_end;
    }
    uint32_t old_brk = g_current_task_ptr->brk_end;
    
    //brk扩容时，不分配页帧，缺页补帧
    if (new_brk < old_brk) {
        uint32_t old_page = JLOS_PAGE_ALIGN_DOWN(old_brk);
        uint32_t new_page = JLOS_PAGE_ALIGN_UP(new_brk);
        for (uint32_t addr = new_page; addr < old_page; addr += JLOS_PAGE_FRAME_SIZE) {
            jlos_paging_unmap(g_current_task_ptr->mm, addr);
        }
    }
    g_current_task_ptr->brk_end = new_brk;
    (void)arg2;
    (void)arg3;
    return (int32_t)new_brk;
}

static int32_t syscall_exit(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    JLOS_TASK_SET_ZOMBIE(g_current_task_ptr, arg1);
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    g_current_task_ptr->yield = true;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_get_pid(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return g_current_task_ptr->pid;
}

static int32_t syscall_sleep(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    g_current_task_ptr->wake_tick = jlos_hal_timer_get_ticks() + arg1;
    g_current_task_ptr->sleeping = true;
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_get_errno(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return g_current_task_ptr->errno;
}

static int32_t syscall_get_ticks(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return (int32_t)jlos_hal_timer_get_ticks();
}

static int32_t syscall_get_tasks_info(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_task_manager_ptr) {
        return - SYSCALL_ENOMEM;
    }
    printf("=== task list ===\n");
    for (int i = 0; i < g_task_manager_ptr->num_tasks; i++) {
        jlos_task_t *t = g_task_manager_ptr->tasks[i];
        if (t) {
            printk("[%d] name = %s, pid = %u, status = %d, task_type = %s\n", i, t->name,
                t->pid, t->status, t->is_user_process ? "user" : "kernel");
        }
    }
    printf("================\n");
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_wait_pid(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t target_pid = arg1;
    int32_t *exit_code = (int32_t *)arg2;

    if (!g_current_task_ptr || !g_task_manager_ptr) {
        return -SYSCALL_ENOMEM;
    }
    jlos_task_t *target = jlos_task_manager_find_pid(g_task_manager_ptr, target_pid);
    if (!target || target->parent_pid != g_current_task_ptr->pid) {
        return -SYSCALL_ENINVAL;
    }
    if (target->status == JLOS_TASK_ZOMBIE) {
        if (exit_code) {
            if (!jlos_copy_to_user(exit_code, &target->exit_code, sizeof(int32_t))) {
                return -SYSCALL_EFAULT;
            }
        }
        uint32_t pid = target->pid;
        jlos_task_free(g_task_manager_ptr, target);
        return (int32_t)pid;
    }
    JLOS_TASK_SET_WAITING(g_current_task_ptr, target_pid);
    (void)arg3;
    return 0;
}

void jlos_syscall_register(uint8_t num, jlos_syscall_func_t handler)
{
    if (!s_syscall_handler) {
        return;
    }
    s_syscall_handler->dispatch[num] = handler;
}

static int32_t s_syscall_dispatch(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3)
{
    return jlos_syscall_do_dispatch(s_syscall_handler, num, a1, a2, a3);
}

static uint32_t s_syscall_resched_do(uint32_t ctx)
{
    if (g_current_task_ptr) {
        g_current_task_ptr->yield = false;
        return (uint32_t)jlos_task_manager_schedule(g_task_manager_ptr, (jlos_cpu_state_t *)ctx);
    }
    return ctx;
}

void jlos_syscall_handler_init(jlos_syscall_handler_t* self, jlos_irq_manager_t *interrupt_manager, uint8_t interrupt_number)
{
    s_syscall_handler = self;
    self->interrupt_manager = interrupt_manager;
    self->interrupt_number = interrupt_number;
    jlos_irq_handler_init((jlos_irq_handler_t *)self, interrupt_manager, interrupt_number);
    
    self->handle_interrupt = (jlos_syscall_handle_interrupt_func_t)jlos_hal_syscall_entry;
    jlos_hal_syscall_dispatch = s_syscall_dispatch;
    jlos_hal_syscall_resched_check = jlos_syscall_need_resched;
    jlos_hal_syscall_resched_do = s_syscall_resched_do;

    for (int i = 0; i < JLOS_SYSCALL_MAX; i++) {
        self->dispatch[i] = 0;
    }
    jlos_syscall_register(JLOS_SYSCALL_WRITE, syscall_write);
    jlos_syscall_register(JLOS_SYSCALL_READ, syscall_read);
    jlos_syscall_register(JLOS_SYSCALL_YIELD, syscall_yield);
    jlos_syscall_register(JLOS_SYSCALL_EXIT, syscall_exit);
    jlos_syscall_register(JLOS_SYSCALL_GET_PID, syscall_get_pid);
    jlos_syscall_register(JLOS_SYSCALL_SLEEP, syscall_sleep);
    jlos_syscall_register(JLOS_SYSCALL_GET_ERRNO, syscall_get_errno);
    jlos_syscall_register(JLOS_SYSCALL_GET_TICKS, syscall_get_ticks);
    jlos_syscall_register(JLOS_SYSCALL_GET_TASKS_INFO, syscall_get_tasks_info);
    jlos_syscall_register(JLOS_SYSCALL_WAIT_PID, syscall_wait_pid);
    jlos_syscall_register(JLOS_SYSCALL_CREATE_PIPE, syscall_create_pipe);
    jlos_syscall_register(JLOS_SYSCALL_TASK_FD_CLOSE, syscall_task_fd_close);
    jlos_syscall_register(JLOS_SYSCALL_TASK_BRK, syscall_task_brk);
}

void jlos_syscall_handler_destroy(jlos_syscall_handler_t* self)
{
    (void)self;
}

int32_t jlos_syscall_do_dispatch(jlos_syscall_handler_t *self, uint32_t syscall_num, uint32_t arg1, uint32_t arg2,
    uint32_t arg3)
{
    int32_t result = - SYSCALL_ENOSYS;
    if (syscall_num < JLOS_SYSCALL_MAX && self->dispatch[syscall_num]) {
        result = self->dispatch[syscall_num](arg1, arg2, arg3);
    }
    if (g_current_task_ptr) {
        g_current_task_ptr->errno = (result < 0) ? -result : 0;
        return (result < 0) ? -1 : result;
    }
    return result;
}

bool jlos_syscall_need_resched()
{
    if (!g_current_task_ptr) {
        return false;
    }
    if (g_current_task_ptr->status == JLOS_TASK_ZOMBIE || g_current_task_ptr->status == JLOS_TASK_WAITING
    || g_current_task_ptr->sleeping || g_current_task_ptr->yield) {
        return true;
    }
    return false;
}

bool jlos_access_ok(const void *addr, size_t n)
{
    jlos_paging_context_t *ctx =
        g_current_task_ptr && g_current_task_ptr->mm ? g_current_task_ptr->mm : jlos_active_paging_context;
    return jlos_paging_is_user_accessible(ctx, (uint32_t)addr, n);
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
