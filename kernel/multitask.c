#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/paging.h>
#include <kernel/page_frame_allocator.h>
#include <hal/context.h>
#include <hal/timer.h>
#include <kernel/printk.h>

extern jlos_task_t *g_current_task_ptr;

jlos_task_manager_t *g_task_manager_ptr = NULL;

static uint32_t s_next_pid = 1;

void jlos_task_set_ready(jlos_task_t *t)
{
    t->status = JLOS_TASK_READY;
    t->last_ready_tick = jlos_hal_timer_get_ticks();
    t->yield = false;
}

void jlos_task_set_running(jlos_task_t *t)
{
    t->status = JLOS_TASK_RUNNING;
    t->remain_slice = t->default_slice;
    t->yield = false;
}

void jlos_task_set_blocked(jlos_task_t *t)
{
    t->status = JLOS_TASK_BLOCKED;
    t->yield = true;
}

void jlos_task_set_terminated(jlos_task_t *t, uint32_t exit_code)
{
    t->status = JLOS_TASK_TERMINATED;
    t->exit_code = exit_code;
}

void jlos_task_set_waiting(jlos_task_t *t, uint32_t pid)
{
    t->status = JLOS_TASK_WAITING;
    t->waiting_pid = pid;
    t->yield = true;
}

void jlos_task_init_1(jlos_task_t *self, const char *name)
{
    if (name) {
        int i;
        for (i = 0; i < JLOS_TASK_NAME_SIZE && name[i]; i++) {
            self->name[i] = name[i];
        }
        self->name[i] = '\0';
    } else {
        self->name[0] = '\0';
    }
    JLOS_TASK_SET_READY(self);
    self->pid = s_next_pid++;
    self->parent_pid = 0;
    self->exit_code = TASK_EXIT_DEAUFT;
    self->mm = NULL;
    self->sleeping = false;
    self->wake_tick = jlos_hal_timer_get_ticks();
    self->errno = 0;
    self->waiting_pid = self->pid;
    self->next_wait = NULL;
    self->priority = 0;
    self->default_slice = (2 << self->priority);
    self->remain_slice = self->default_slice;
}

void jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name)
{
    jlos_task_init_1(self, name);
    self->stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->stack_size = JLOS_TASK_STACK_SIZE;
    self->is_user_process = false;
    self->user_stack = NULL;
    self->user_stack_size = 0;
    self->fds = NULL;
    self->fds_size = 0;
    self->brk_start = 0;
    self->brk_end = 0;
    self->brk_limit = 0;
    if (self->stack) {
        jlos_memset(self->stack, 0, JLOS_TASK_STACK_SIZE);
    }

    jlos_cpu_state_init(&self->cpustate);
    jlos_arch_task_init_arch(&self->cpustate, mmu, entrypoint, self->stack, self->stack_size);
}

void jlos_task_init_user(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name)
{
    jlos_task_init_1(self, name);
    self->stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    self->stack_size = JLOS_TASK_STACK_SIZE;
    self->fds = (jlos_task_fd_t *)jlos_malloc(sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
    self->fds_size = 0;
    if (self->fds) {
        for (int i = 0; i < JLOS_TASK_FDS_NUM; i++) {
            self->fds[i].type = JLOS_TASK_FD_UNUSED;
            self->fds[i].obj = NULL;
            self->fds[i].flags = 0;
        }
        self->fds_size = JLOS_TASK_FDS_NUM;
    }
    for (uint32_t i = 0; i < 3 && i < self->fds_size; i++) {
        self->fds[i].type = JLOS_TASK_FD_CONSOLE;
        self->fds[i].obj = NULL;
        self->fds[i].flags = i ? JLOS_TASK_FD_WRITE_ONLY : JLOS_TASK_FD_READ_ONLY;
    }
    self->brk_start = JLOS_TASK_USER_BRK_START;
    self->brk_end = self->brk_start;
    self->brk_limit = JLOS_TASK_USER_BRK_LIMIT;
    self->is_user_process = true;
    if (self->stack) {
        jlos_memset(self->stack, 0, JLOS_TASK_STACK_SIZE);
    }

    /* 用户栈映射到用户地址空间 (经典 3:1 分离) */
    uint32_t stack_base = JLOS_TASK_USER_STACK_TOP - JLOS_TASK_USER_STACK_SIZE;
    self->user_stack = (uint8_t *)stack_base;
    self->user_stack_size = JLOS_TASK_USER_STACK_SIZE;
    for (uint32_t addr = stack_base; addr < JLOS_TASK_USER_STACK_TOP; addr += JLOS_PAGE_FRAME_SIZE) {
        void *frame = jlos_page_frame_malloc();
        if (!frame) { break; }
        jlos_paging_map(jlos_active_paging_context, addr, VIRT_TO_PHYS((uint32_t)frame),
            JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE | JLOS_PTE_USER);
    }

    /* .text/.rodata 加 PTE_USER：ring3 syscall wrapper 需取指和读常量 */
    const jlos_hal_kernel_segments_t *seg = jlos_hal_get_kernel_segments();
    if (seg && seg->text_start != 0) {
        jlos_paging_change_flags_range(jlos_active_paging_context,
            JLOS_PAGE_ALIGN_DOWN(seg->text_start),
            JLOS_PAGE_ALIGN_DOWN(seg->text_end + JLOS_PAGE_SIZE - 1),
            JLOS_PTE_PRESENT | JLOS_PTE_USER);
        if (seg->rodata_start != 0) {
            jlos_paging_change_flags_range(jlos_active_paging_context,
                JLOS_PAGE_ALIGN_DOWN(seg->rodata_start),
                JLOS_PAGE_ALIGN_DOWN(seg->rodata_end + JLOS_PAGE_SIZE - 1),
                JLOS_PTE_PRESENT | JLOS_PTE_USER);
        }
    }

    jlos_cpu_state_init(&self->cpustate);
    jlos_arch_task_init_arch_user(&self->cpustate, mmu, entrypoint,
        self->stack, self->stack_size, JLOS_TASK_USER_STACK_TOP, 0x2B);
}

void jlos_task_free(jlos_task_manager_t *self, jlos_task_t *task)
{
    if (!self || !task) {
        return;
    }
    if (task->stack) {
        jlos_free(task->stack);
        task->stack = NULL;
    }
    if (task->user_stack) {
        uint32_t base = JLOS_PAGE_ALIGN_DOWN((uint32_t)task->user_stack);
        uint32_t top = base + task->user_stack_size;
        for (uint32_t addr = base; addr < top; addr += JLOS_PAGE_FRAME_SIZE) {
            jlos_paging_unmap(jlos_active_paging_context, addr);
        }
        task->user_stack = NULL;
    }
    if (task->is_user_process && task->mm) {
        jlos_free(task->mm);
        task->mm = NULL;
    }
    if (task->fds) {
        jlos_free(task->fds);
        task->fds = NULL;
    }
    for (int i = 0; i < self->num_tasks; i++) {
        if (self->tasks[i] == task) {
            self->tasks[i] = self->tasks[self->num_tasks - 1];
            self->tasks[self->num_tasks - 1] = NULL;
            self->num_tasks--;
            break;
        }
    }
    jlos_free(task);
}

int32_t jlos_task_fd_malloc(jlos_task_t *task)
{
    for (int i = 3; i < JLOS_TASK_FDS_NUM; i++) {
        if (task->fds[i].type == JLOS_TASK_FD_UNUSED) {
            task->fds[i].type = JLOS_TASK_FD_PIPE;
            return i;
        }
    }
    return -1;
}

jlos_task_fd_t *jlos_task_fd_get(jlos_task_t *task, int32_t fd)
{
    if (fd < 0 || fd >= JLOS_TASK_FDS_NUM) {
        return NULL;
    }
    if (task->fds[fd].type == JLOS_TASK_FD_UNUSED) {
        return NULL;
    }
    return &task->fds[fd];
}
void jlos_task_fd_free(jlos_task_t *task, int32_t fd)
{
    if (fd < 0 || fd >= JLOS_TASK_FDS_NUM) {
        return;
    }
    task->fds[fd].type = JLOS_TASK_FD_UNUSED;
    task->fds[fd].obj = NULL;
    task->fds[fd].flags = 0;
}

void jlos_task_manager_init(jlos_task_manager_t* self)
{
    self->num_tasks = 0;
    self->current_task = -1;
    self->main_thread_saved = false;
    g_task_manager_ptr = self;
}

void jlos_task_manager_destroy(jlos_task_manager_t* self)
{
    for (int i = self->num_tasks - 1; i >= 0; i--) {
        jlos_task_free(self, self->tasks[i]);
    }
    self->num_tasks = 0;
    self->current_task = -1;
    self->main_thread_saved =false;
    g_task_manager_ptr = NULL;
}

bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task)
{
    if (self->num_tasks >= 256) {
        return false;
    }
    self->tasks[self->num_tasks++] = task;
    return true;
}

jlos_cpu_state_t *jlos_task_manager_schedule(jlos_task_manager_t* self, jlos_cpu_state_t *cpustate)
{
    if (self == NULL || cpustate == NULL) return cpustate;
    if (self->num_tasks <= 0) {
        g_current_task_ptr = NULL;
        return cpustate;
    }

    /* 如果当前是主线程（current_task == -1），保存主线程的状态 */
    if (self->current_task == -1) {
        if (!self->main_thread_saved) {
            jlos_memcpy(&self->main_thread_state, cpustate, sizeof(jlos_cpu_state_t));
            if (!jlos_cpu_state_is_user_mode(cpustate)) {
                jlos_cpu_state_record_user_stack(&self->main_thread_state, cpustate);
            }
            self->main_thread_saved = true;
        }
    } else if (self->current_task < (int)self->num_tasks) {
        jlos_task_t *current = self->tasks[self->current_task];
        if (current && current->status == JLOS_TASK_RUNNING) {
            if (current->yield || current->sleeping) {
                JLOS_TASK_SET_READY(current);
            } else
            if (current->remain_slice == 0) {
                if (current->priority < JLOS_TASK_MLFQ_LEVELS - 1) {
                    current->priority++;
                    current->default_slice = 2 << current->priority;
                }
                JLOS_TASK_SET_READY(current);
            }
            jlos_memcpy(&current->cpustate, cpustate, sizeof(jlos_cpu_state_t));
            if (!jlos_cpu_state_is_user_mode(cpustate)) {
                jlos_cpu_state_record_user_stack(&current->cpustate, cpustate);
            }
        }
    }
    uint32_t now_tick = jlos_hal_timer_get_ticks();
    for (int i = 0; i < self->num_tasks; i++) {
        jlos_task_t *t = self->tasks[i];
        if (!t) {
            continue;
        }
        if (t->sleeping && t->wake_tick <= now_tick) {
            t->sleeping = false;
            t->wake_tick = now_tick;
        }
        if (t->status == JLOS_TASK_WAITING && t->waiting_pid != t->pid) {
            for (int j = 0; j < self->num_tasks; j++) {
                jlos_task_t *child = self->tasks[j];
                if (child && child->pid == t->waiting_pid
                && child->status == JLOS_TASK_TERMINATED) {
                    t->waiting_pid = t->pid;
                    JLOS_TASK_SET_READY(t);
                    t->last_ready_tick = now_tick;
                    break;
                }
            }
        }
        if (t->status == JLOS_TASK_READY && (now_tick - t->last_ready_tick) > JLOS_TASK_MLFQ_AGING_TICKS
        && t->priority > 0) {
            t->priority--;
            t->default_slice = (2 << t->priority);
            t->last_ready_tick = now_tick;
        }
    }

    for (uint32_t le = 0; le < JLOS_TASK_MLFQ_LEVELS; le++) {
        for (int i = 0; i < self->num_tasks; i++) {
            int idx = (self->current_task + 1 + i) % self->num_tasks;
            jlos_task_t *next = self->tasks[idx];
            if (next && next->status == JLOS_TASK_READY && next->priority == le
            && !next->sleeping) {
                self->current_task = idx;
                if (next->mm) {
                    jlos_paging_switch(next->mm);
                }
                JLOS_TASK_SET_RUNNING(next);
                g_current_task_ptr = next;
                jlos_arch_tss_set_ctx((uint32_t)(next->stack + next->stack_size));
                return &next->cpustate;
            }
        }
    }

    /* 没有Ready任务，恢复主线程的状态 */
    g_current_task_ptr = NULL;
    self->current_task = -1;
    if (self->main_thread_saved) {
        self->main_thread_saved = false;
        return &self->main_thread_state;
    }
    return cpustate;
}

jlos_task_t *jlos_task_manager_curr_task_on_tick(jlos_task_manager_t *self)
{
    if (!self) {
        return NULL;
    }
    if (self->current_task < 0 || self->current_task >= self->num_tasks) {
        return NULL;
    }
    jlos_task_t *curr = self->tasks[self->current_task];
    if (curr && curr->status == JLOS_TASK_RUNNING) {
        if (curr->remain_slice > 0) {
            curr->remain_slice--;
        }
    }
    return curr;
}

jlos_task_t *jlos_process_fork(jlos_task_manager_t *self, jlos_task_t *parent)
{
    if (self->num_tasks >= 256) {
        return NULL;
    }
    jlos_task_t *child = (jlos_task_t *)jlos_malloc(sizeof(jlos_task_t));
    if (!child) {
        return NULL;
    }
    *child = *parent;
    child->stack = (uint8_t *)jlos_malloc(JLOS_TASK_STACK_SIZE);
    child->stack_size = JLOS_TASK_STACK_SIZE;
    if (child->stack && parent->stack) {
        jlos_memcpy(child->stack, parent->stack, JLOS_TASK_STACK_SIZE);
    }
    child->pid = s_next_pid++;
    JLOS_TASK_SET_READY(child);
    child->parent_pid = parent->pid;
    child->waiting_pid = child->pid;
    child->sleeping = false;
    child->wake_tick = jlos_hal_timer_get_ticks();
    child->remain_slice = parent->default_slice;
    child->next_wait = NULL;
    if (parent->fds) {
        child->fds = (jlos_task_fd_t *)jlos_malloc(sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
        if (child->fds) {
            jlos_memcpy(child->fds, parent->fds, sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
            child->fds_size = JLOS_TASK_FDS_NUM;
        } else {
            child->fds = NULL;
            child->fds_size = 0;
        }
    } else {
        child->fds = NULL;
        child->fds_size = 0;
    }
    child->brk_start = JLOS_TASK_USER_BRK_START;
    child->brk_end = JLOS_TASK_USER_BRK_START;
    child->brk_limit = JLOS_TASK_USER_BRK_LIMIT;
    child->exit_code = TASK_EXIT_DEAUFT;
    if (parent->mm) {
        child->mm = (jlos_paging_context_t *)jlos_malloc(sizeof(jlos_paging_context_t));
        jlos_paging_context_init(child->mm);
    } else {
        child->mm = NULL;
    }
    self->tasks[self->num_tasks++] = child;
    return child;
}

int jlos_process_exec(jlos_task_t *task, void (*entrypoint)(void))
{
    if (task->stack) {
        jlos_free(task->stack);
        task->stack = NULL;
    }
    if (task->user_stack) {
        uint32_t base = JLOS_PAGE_ALIGN_DOWN((uint32_t)task->user_stack);
        uint32_t top = base + task->user_stack_size;
        for (uint32_t addr = base; addr < top; addr += JLOS_PAGE_FRAME_SIZE) {
            jlos_paging_unmap(jlos_active_paging_context, addr);
        }
        task->user_stack = NULL;
    }
    jlos_mmu_t *mmu = jlos_mmu_get_kernel();
    jlos_task_init_user(task, mmu, entrypoint, task->name);
    task->pid = s_next_pid++;
    task->parent_pid = 0;
    task->exit_code = TASK_EXIT_DEAUFT;
    return 0;
}

void jlos_process_exit(jlos_task_t *task, uint32_t exit_code)
{
    JLOS_TASK_SET_TERMINATED(task, exit_code);
}
