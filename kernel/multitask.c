#include <hal/context.h>
#include <hal/timer.h>
#include <hal/cpu_state.h>
#include <hal/hal.h>
#include <hal/hal_arch.h>
#include <hal/paging.h>
#include <hal/kernel_syscall.h>
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/paging.h>
#include <kernel/ipc.h>
#include <kernel/initcall.h>
#include <filesystem/elf.h>
#include <filesystem/vfs.h>

#define JLOS_KERNEL_LOG_SUBSYS "sched"
#include <kernel/printk.h>

extern jlos_task_t              *g_current_task_ptr;
extern jlos_paging_context_t    s_kernel_paging_context;

static jlos_task_manager_t      s_task_manager;
jlos_task_manager_t             *g_task_manager_ptr = &s_task_manager;

static uint32_t                 s_next_pid = 1;

static inline bool task_on_rq(jlos_task_t *t)
{
    return !jlos_list_empty(&t->rq_node);
}

static inline uint32_t clamp_pri(uint32_t p)
{
    return p < JLOS_TASK_MLFQ_LEVELS ? p : (JLOS_TASK_MLFQ_LEVELS - 1);
}

static int pid_hash_cmp(const void *key, const void *node)
{
    uint32_t pid = *(const uint32_t *)key;
    const jlos_task_t *t = container_of(node, const jlos_task_t, pid_hash_node);
    return (t->pid == pid) ? 0 : 1;
}

static void rq_enqueue(jlos_task_manager_t *self, jlos_task_t *t)
{
    uint32_t l = clamp_pri(t->priority);
    jlos_list_add_tail(&t->rq_node, &self->rq[l].head);
    self->rq[l].count++;
    self->rq_nonempty |= (1U << l);
}

static jlos_task_t *rq_dequeue(jlos_task_manager_t *self)
{
    if (!self->rq_nonempty) {
        return NULL;
    }
    int level = __builtin_ctz(self->rq_nonempty);
    jlos_list_head_t *first = self->rq[level].head.next;
    jlos_list_del_init(first);
    if (--self->rq[level].count == 0) {
        self->rq_nonempty &= ~(1U << level);
    }
    jlos_task_t *t = container_of(first, jlos_task_t, rq_node);
    uint32_t now = jlos_hal_timer_get_ticks();
    uint32_t boost = (now - t->last_ready_tick) / JLOS_TASK_MLFQ_AGING_TICKS;
    boost = JLOS_CAP_UPPER(boost, (uint32_t)level);
    level -= boost;
    t->priority = level;
    t->default_slice = (2U << level);
    t->last_ready_tick = now;
    return t;
}

static void rq_remove_specific(jlos_task_manager_t *self, jlos_task_t *t)
{
    if (!t || !task_on_rq(t)) {
        return;
    }
    uint32_t l = clamp_pri(t->priority);
    jlos_list_del_init(&t->rq_node);
    if (--self->rq[l].count == 0) {
        self->rq_nonempty &= ~(1U << l);
    }
}

void jlos_task_set_ready(jlos_task_t *t)
{
    if (!t) {
        return;
    }
    t->status = JLOS_TASK_READY;
    t->last_ready_tick = jlos_hal_timer_get_ticks();
    t->yield = false;
}

void jlos_task_set_running(jlos_task_t *t)
{
    if (!t) {
        return;
    }
    t->status = JLOS_TASK_RUNNING;
    t->remain_slice = t->default_slice;
    t->yield = false;
}

void jlos_task_set_blocked(jlos_task_t *t)
{
    if (!t) {
        return;
    }
    t->status = JLOS_TASK_BLOCKED;
    if (g_task_manager_ptr && task_on_rq(t)) {
        rq_remove_specific(g_task_manager_ptr, t);
    }
}

/* 进程退出 → ZOMBIE: 保留 PCB 等待父进程通过 wait_pid 收割 */
void jlos_task_set_zombie(jlos_task_t *t, uint32_t exit_code)
{
    if (!t) {
        return;
    }
    t->status = JLOS_TASK_ZOMBIE;
    t->exit_code = exit_code;
    if (g_task_manager_ptr && task_on_rq(t)) {
        rq_remove_specific(g_task_manager_ptr, t);
    }
}

void jlos_task_set_waiting(jlos_task_t *t, uint32_t pid)
{
    if (!t) {
        return;
    }
    jlos_task_t *target = jlos_task_manager_find_pid(g_task_manager_ptr, pid);
    if (target && target->status == JLOS_TASK_ZOMBIE) {
        return;
    }
    t->status = JLOS_TASK_WAITING;
    t->waiting_pid = pid;
    if (g_task_manager_ptr && task_on_rq(t)) {
        rq_remove_specific(g_task_manager_ptr, t);
    }
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
    self->exit_code = TASK_EXIT_DEFAULT;
    self->mm = NULL;
    self->sleeping = false;
    self->wake_tick = jlos_hal_timer_get_ticks();
    self->errno = 0;
    self->waiting_pid = self->pid;
    self->priority = 0;
    self->default_slice = (2 << self->priority);
    self->remain_slice = self->default_slice;
    self->pid_hash_node.next = NULL;
    self->pid_hash_node.pprev = NULL;
    jlos_list_init(&self->wait_node);
    jlos_list_init(&self->zombie_node);
    jlos_list_init(&self->rq_node);
    self->slot_idx = -1;
    jlos_arch_task_ext_init(self);
}

int32_t jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name)
{
    jlos_task_init_1(self, name);
    self->stack = (uint8_t *)jlos_kalloc(JLOS_TASK_STACK_SIZE);
    if (!self->stack) {
        return -TASK_ERR_NOMEM;
    }
    self->stack_size = JLOS_TASK_STACK_SIZE;
    self->is_user_process = false;
    self->user_stack = NULL;
    self->user_stack_size = 0;
    self->fds = NULL;
    jlos_memset(self->stack, 0, JLOS_TASK_STACK_SIZE);

    jlos_cpu_state_init(&self->cpustate);
    jlos_arch_task_init_arch(&self->cpustate, mmu, entrypoint, self->stack, self->stack_size);
    return 0;
}

static int32_t jlos_task_create_user_mm(jlos_task_t *self)
{
    if (!self) {
        return -TASK_ERR_NOMEM;
    }
    self->mm = jlos_mm_create();
    if (!self->mm) {
        return -TASK_ERR_NOMEM;
    }
    jlos_paging_context_clone(self->mm->pc, &s_kernel_paging_context);
    self->mm->brk_start = JLOS_TASK_USER_BRK_START;
    self->mm->brk_end = self->mm->brk_start;
    self->mm->brk_limit = JLOS_TASK_USER_BRK_LIMIT;
    return 0;
}

static int32_t jlos_task_create_user_stack(jlos_task_t *self)
{
    if (!self || !self->mm) {
        return -TASK_ERR_NOMEM;
    }
    uint32_t stack_base = JLOS_TASK_USER_STACK_TOP - JLOS_TASK_USER_STACK_SIZE;
    self->user_stack = (uint8_t *)stack_base;
    self->user_stack_size = JLOS_TASK_USER_STACK_SIZE;
    jlos_vma_add(self->mm, stack_base,
        JLOS_TASK_USER_STACK_TOP + JLOS_PAGE_FRAME_SIZE, JLOS_VMA_WRITE | JLOS_VMA_USER, JLOS_VMA_TYPE_STACK);
    return 0;
}

int32_t jlos_task_init_user(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name)
{
    jlos_task_init_1(self, name);
    self->stack = (uint8_t *)jlos_kalloc(JLOS_TASK_STACK_SIZE);
    if (!self->stack) {
        return -TASK_ERR_NOMEM;
    }
    self->stack_size = JLOS_TASK_STACK_SIZE;
    self->fds = (jlos_task_fd_t *)jlos_kalloc(sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
    if (self->fds) {
        for (uint32_t i = 0; i < 3 && i < JLOS_TASK_FDS_NUM; i++) {
            self->fds[i].type = JLOS_TASK_FD_CONSOLE;
            self->fds[i].obj = NULL;
            self->fds[i].flags = i ? JLOS_TASK_FD_WRITE_ONLY : JLOS_TASK_FD_READ_ONLY;
        }
        for (uint32_t i = 3; i < JLOS_TASK_FDS_NUM; i++) {
            self->fds[i].type = JLOS_TASK_FD_UNUSED;
            self->fds[i].obj = NULL;
            self->fds[i].flags = 0;
        }
    }
    self->is_user_process = true;
    jlos_memset(self->stack, 0, JLOS_TASK_STACK_SIZE);

    int32_t ret = jlos_task_create_user_mm(self);
    if (ret < 0) {
        if (self->mm) {
            jlos_mm_destroy(self->mm);
            self->mm = NULL;
        }
        jlos_kfree(self->fds);
        jlos_kfree(self->stack);
        return ret;
    }
    ret = jlos_task_create_user_stack(self);
    if (ret < 0) {
        jlos_mm_destroy(self->mm);
        self->mm = NULL;
        jlos_kfree(self->fds);
        jlos_kfree(self->stack);
        return ret;
    }
    jlos_cpu_state_init(&self->cpustate);
    jlos_arch_task_init_arch_user(&self->cpustate, mmu, entrypoint,
        self->stack, self->stack_size, JLOS_TASK_USER_STACK_TOP);
    return 0;
}

static void jlos_task_unmap_user_stack(jlos_task_t *task)
{
    if (!task->user_stack) {
        return;
    }
    uint32_t base = JLOS_PAGE_ALIGN_DOWN((uint32_t)task->user_stack);
    uint32_t top = base + task->user_stack_size;
    if (task->mm) {
        jlos_vma_remove_range(task->mm, base, top);
    } else {
        for (uint32_t addr = base; addr < top; addr += JLOS_PAGE_FRAME_SIZE) {
            jlos_paging_unmap(jlos_hal_paging_get_active_context(), addr);
        }
    }
    task->user_stack = NULL;
}

void jlos_task_free(jlos_task_manager_t *self, jlos_task_t *task)
{
    if (!self || !task) {
        return;
    }
    jlos_hash_chain_remove(&self->pid_hash, &task->pid_hash_node);
    jlos_arch_task_ext_destroy(task);
    if (task->stack && task->pid != 0) {
        jlos_kfree(task->stack);
        task->stack = NULL;
    }
    if (task->mm) {
        jlos_mm_destroy(task->mm);
        task->mm = NULL;
    } else {
        jlos_task_unmap_user_stack(task);
    }
    if (task->fds) {
        jlos_kfree(task->fds);
        task->fds = NULL;
    }
    int32_t idx = task->slot_idx;
    if (idx >= 0 && (uint32_t)idx < self->num_tasks && self->tasks[idx] == task) {
        self->tasks[idx] = self->tasks[self->num_tasks - 1];
        if (self->tasks[idx]) {
            self->tasks[idx]->slot_idx = idx;
        }
        self->tasks[self->num_tasks - 1] = NULL;
        self->num_tasks--;
    }
    if (g_current_task_ptr == task) {
        g_current_task_ptr = NULL;
    }
    task->slot_idx = -1;
    jlos_list_del_init(&task->wait_node);
    jlos_list_del_init(&task->zombie_node);
    jlos_list_del_init(&task->rq_node);
    jlos_kfree(task);
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

void jlos_task_manager_init()
{
    jlos_task_manager_t *self = &s_task_manager;
    self->max_tasks = JLOS_TASK_MAX_NUM;
    self->tasks = (jlos_task_t **)jlos_kalloc(sizeof(jlos_task_t *) * self->max_tasks);
    if (!self->tasks) {
        goto halt;
    }
    jlos_memset(self->tasks, 0, sizeof(jlos_task_t *) * self->max_tasks);
    jlos_hash_chain_init(&self->pid_hash, JLOS_TASK_PID_HASH_SIZE, jlos_hash_uint32, pid_hash_cmp);
    for (uint32_t l = 0; l < JLOS_TASK_MLFQ_LEVELS; l++) {
        jlos_list_init(&self->rq[l].head);
        self->rq[l].count = 0;
    }
    jlos_list_init(&self->zombie_head);
    jlos_list_init(&self->sleep_queue);
    self->need_resched = false;
    self->rq_nonempty = 0;
    self->idle_task = (jlos_task_t *)jlos_kalloc(sizeof(jlos_task_t));
    if (!self->idle_task) {
        goto halt;
    }
    jlos_task_init_1(self->idle_task, "idle");
    self->idle_task->pid = 0;
    self->idle_task->mm = NULL;
    self->idle_task->is_user_process = false;
    self->idle_task->fds = NULL;
    self->idle_task->user_stack = NULL;
    self->idle_task->user_stack_size = 0;
    uint8_t *boot_base = NULL;
    uint32_t boot_size = 0;
    jlos_arch_boot_stack_info(&boot_base, &boot_size);
    self->idle_task->stack = boot_base;
    self->idle_task->stack_size = boot_size;
    self->tasks[0] = self->idle_task;
    self->idle_task->slot_idx = 0;
    self->num_tasks = 1;
    self->current_task = 0;
    JLOS_TASK_SET_RUNNING(self->idle_task);
    jlos_hash_chain_insert(&self->pid_hash, &self->idle_task->pid, &self->idle_task->pid_hash_node);
    g_current_task_ptr = self->idle_task;
    return;
halt:
    for (;;) {
        jlos_hal_halt();
    }
}

void jlos_task_manager_destroy(jlos_task_manager_t* self)
{
    while (self->num_tasks) {
        jlos_task_free(self, self->tasks[self->num_tasks - 1]);
    }
    jlos_hash_chain_destroy(&self->pid_hash);
    jlos_kfree(self->tasks);
    self->tasks = NULL;
    self->max_tasks = 0;
    self->num_tasks = 0;
    self->current_task = -1;
    g_task_manager_ptr = NULL;
}

jlos_task_t *jlos_task_manager_find_pid(jlos_task_manager_t *self, uint32_t pid)
{
    if (!self) {
        return NULL;
    }
    jlos_hash_node_t *node = jlos_hash_chain_see(&self->pid_hash, &pid);
    if (!node) {
        return NULL;
    }
    return container_of(node, jlos_task_t, pid_hash_node);
}

bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task)
{
    if (self->num_tasks >= self->max_tasks) {
        uint32_t new_max = self->max_tasks << 1;
        jlos_task_t **new_tasks = (jlos_task_t **)jlos_kalloc(sizeof(jlos_task_t *) * new_max);
        if (!new_tasks) {
            return false;
        }
        jlos_memcpy(new_tasks, self->tasks, sizeof(jlos_task_t *) * self->max_tasks);
        jlos_memset(new_tasks + self->max_tasks, 0, sizeof(jlos_task_t *) * (new_max - self->max_tasks));
        jlos_kfree(self->tasks);
        self->tasks = new_tasks;
        self->max_tasks = new_max;
    }
    self->tasks[self->num_tasks] = task;
    task->slot_idx = self->num_tasks;
    self->num_tasks++;
    if (task->status == JLOS_TASK_READY) {
        rq_enqueue(self, task);
    }
    jlos_hash_chain_insert(&self->pid_hash, &task->pid, &task->pid_hash_node);
    return true;
}

void jlos_task_manager_schedule(jlos_task_manager_t* self)
{
    if (!self || !g_current_task_ptr) {
        return;
    }
    jlos_task_t *prev = g_current_task_ptr;
    if (prev->status == JLOS_TASK_RUNNING) {
        if (prev->sleeping) {
            JLOS_TASK_SET_BLOCKED(prev);
        } else if (prev != self->idle_task) {
            if (prev->yield || !prev->remain_slice) {
                if (!prev->remain_slice && prev->priority < JLOS_TASK_MLFQ_LEVELS - 1) {
                    prev->priority++;
                    prev->default_slice = (2U << prev->priority);
                }
            }
            JLOS_TASK_SET_READY(prev);
            rq_enqueue(self, prev);
        }
    }

    if (!jlos_list_empty(&self->zombie_head)) {
        jlos_list_head_t *pos, *n;
        jlos_list_for_each_safe(pos, n, &self->zombie_head) {
            jlos_task_t *z = container_of(pos, jlos_task_t, zombie_node);
            jlos_list_del_init(pos);
            jlos_task_free(self, z);
        }
    }

    uint32_t now_tick = jlos_hal_timer_get_ticks();
    while (!jlos_list_empty(&self->sleep_queue)) {
        jlos_task_t *t = container_of(self->sleep_queue.next, jlos_task_t, wait_node);
        if (t->wake_tick > now_tick) {
            break;
        }
        jlos_list_del_init(&t->wait_node);
        t->sleeping = false;
        if (t->status == JLOS_TASK_BLOCKED) {
            JLOS_TASK_SET_READY(t);
            rq_enqueue(self, t);
        }
    }

    jlos_task_t *next = rq_dequeue(self);
    if (!next) {
        next = self->idle_task;
    }
    if (next == prev) {
        JLOS_TASK_SET_RUNNING(prev);
        self->need_resched = false;
        return;
    }
    
    int32_t idx = next->slot_idx;
    if (idx < 0 || (uint32_t)idx >= self->num_tasks || self->tasks[idx] != next) {
        idx = -1;
        for (uint32_t i = 0; i < self->num_tasks; i++) {
            if (self->tasks[i] == next) {
                idx = i;
                next->slot_idx = i;
                break;
            }
        }
    }
    if (idx < 0) {
        printk_err("[sched] next = %p not in tasks[], fallback failed\n", next);
        return;
    }

    jlos_paging_switch(next->mm ? next->mm->pc : &s_kernel_paging_context);
    jlos_arch_tss_set_ctx((uint32_t)(next->stack + next->stack_size));
    jlos_arch_task_ext_switch();

    self->current_task = next->slot_idx;
    g_current_task_ptr = next;
    JLOS_TASK_SET_RUNNING(next);
    self->need_resched = false;

    jlos_hal_context_switch(&prev->sp.value, next->sp.value);
}

jlos_task_t *jlos_task_manager_curr_task_on_tick(jlos_task_manager_t *self)
{
    if (!self) {
        return NULL;
    }
    if (self->current_task < 0 || ((uint32_t)self->current_task >= self->num_tasks)) {
        return NULL;
    }
    jlos_task_t *curr = self->tasks[self->current_task];
    if (curr == self->idle_task) {
        return NULL;
    }
    if (curr && curr->status == JLOS_TASK_RUNNING) {
        if (curr->remain_slice > 0) {
            curr->remain_slice--;
        }
    }
    return curr;
}

jlos_task_t *jlos_process_fork(jlos_task_manager_t *self, jlos_task_t *parent, uint32_t fork_esp_ref, uint32_t fork_resume_pc)
{
    if (self->num_tasks >= JLOS_TASK_MAX_NUM) {
        return NULL;
    }

    jlos_task_t *child = (jlos_task_t *)jlos_kalloc(sizeof(jlos_task_t));
    if (!child) {
        return NULL;
    }
    if (!JLOS_KERN_PTR_VALID(child, sizeof(jlos_task_t))) {
        printk_err("[fk-OV] task=%p (sz=%u) below writable min; rollback OOM\n",
               child, sizeof(jlos_task_t));
        jlos_kfree(child);
        return NULL;
    }
    *child = *parent;
    jlos_arch_task_ext_init(child);
    child->stack = (uint8_t *)jlos_kalloc(JLOS_TASK_STACK_SIZE);
    child->stack_size = JLOS_TASK_STACK_SIZE;
    if (child->stack) {
        if (!JLOS_KERN_PTR_VALID(child->stack, JLOS_TASK_STACK_SIZE)) {
            printk_err("[fk-OV] stack=%p (sz=%u) below writable min; rollback OOM\n",
                   child->stack, (unsigned)JLOS_TASK_STACK_SIZE);
            jlos_kfree(child->stack);
            child->stack = NULL;
        }
    }
    if (child->stack && parent->stack) {
        jlos_memcpy(child->stack, parent->stack, JLOS_TASK_STACK_SIZE);
    }

    child->pid = s_next_pid++;
    JLOS_TASK_SET_READY(child);
    child->parent_pid = parent->pid;
    child->waiting_pid = child->pid;
    child->sleeping = false;
    child->wake_tick = jlos_hal_timer_get_ticks();
    child->errno = 0;
    child->exit_code = TASK_EXIT_DEFAULT;
    child->remain_slice = parent->default_slice;
    child->pid_hash_node.next = NULL;
    child->pid_hash_node.pprev = NULL;
    child->slot_idx = -1;
    jlos_list_init(&child->wait_node);
    jlos_list_init(&child->rq_node);
    jlos_list_init(&child->zombie_node);
    
    if (parent->fds) {
        child->fds = (jlos_task_fd_t *)jlos_kalloc(sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
        if (child->fds) {
            jlos_memcpy(child->fds, parent->fds, sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
        } else {
            child->fds = NULL;
        }
    } else {
        child->fds = NULL;
    }

    if (!child->stack || (parent->fds && !child->fds)) {
        goto ROLLBACK_OOM;
    }

    if (child->is_user_process && jlos_task_create_user_mm(child) < 0) {
ROLLBACK_OOM:
        jlos_kfree(child->stack);
        jlos_kfree(child->fds);
        jlos_kfree(child);
        child = NULL;
        return NULL;
    }
    if (parent->mm && child->mm) {
        jlos_mm_clone_user(child->mm, parent->mm);
    }
    
    jlos_arch_task_fork_prepare_child(parent->stack, child->stack, fork_esp_ref, fork_resume_pc, child);
    if (!jlos_task_manager_add_task(self, child)) {
        jlos_kfree(child->stack);
        jlos_kfree(child->fds);
        if (child->mm) {
            jlos_mm_destroy(child->mm);
            child->mm = NULL;
        }
        jlos_kfree(child);
        return NULL;
    }
    if (child->fds) {
        for (int i = 0; i < JLOS_TASK_FDS_NUM; i++) {
            if (child->fds[i].type == JLOS_TASK_FD_PIPE && child->fds[i].obj) {
                jlos_pipe_ref_inc((jlos_pipe_t *)child->fds[i].obj);
            }
        }
    }
    return child;
}

int jlos_process_exec(jlos_task_t *task, void (*entrypoint)(void))
{
    if (!task) {
        return 0;
    }
    if (g_task_manager_ptr) {
        jlos_hash_chain_remove(&g_task_manager_ptr->pid_hash, &task->pid_hash_node);
    }
    if (task->mm) {
        jlos_mm_destroy(task->mm);
        task->mm = NULL;
    }
    if (task->stack) {
        jlos_kfree(task->stack);
        task->stack = NULL;
    }
    jlos_task_unmap_user_stack(task);
    jlos_mmu_t *mmu = jlos_mmu_get_kernel();
    int32_t err_code = jlos_task_init_user(task, mmu, entrypoint, task->name);
    if (err_code < 0) {
        jlos_process_exit(task, (uint32_t)-err_code);
        return -1;
    }
    if (g_task_manager_ptr) {
        jlos_hash_chain_insert(&g_task_manager_ptr->pid_hash, &task->pid, &task->pid_hash_node);
    }
    task->parent_pid = 0;
    task->exit_code = TASK_EXIT_DEFAULT;
    return 0;
}

void jlos_process_exit(jlos_task_t *task, uint32_t exit_code)
{   
    if (!task || !g_task_manager_ptr) {
        goto halt;
    }
    JLOS_TASK_SET_ZOMBIE(task, exit_code);
    jlos_sched_wake_waiter(g_task_manager_ptr, task->pid);

    g_task_manager_ptr->need_resched = true;
    jlos_task_manager_schedule(g_task_manager_ptr);
halt:
    for (;;) {
        jlos_hal_halt();
    }
}

bool jlos_need_resched(void)
{
    return g_task_manager_ptr && g_task_manager_ptr->need_resched;
}

void jlos_sched_set_need_resched(void)
{
    if (g_task_manager_ptr) {
        g_task_manager_ptr->need_resched = true;
    }
}

void jlos_sched_wake_waiter(jlos_task_manager_t *self, uint32_t exited_pid)
{
    if (!self) {
        return;
    }
    jlos_task_t *exited = jlos_task_manager_find_pid(self, exited_pid);
    if (exited) {
        if (exited->parent_pid && exited->parent_pid != exited_pid) {
            jlos_task_t *parent = jlos_task_manager_find_pid(self, exited->parent_pid);
            if (parent && parent->status == JLOS_TASK_WAITING && parent->waiting_pid == exited_pid) {
                parent->waiting_pid = parent->pid;
                JLOS_TASK_SET_READY(parent);
                if (!task_on_rq(parent)) {
                    rq_enqueue(self, parent);
                }
                self->need_resched = true;
            }
        }
    }
}

void jlos_task_sleep_until(jlos_task_manager_t *self, uint32_t wake_tick)
{
    if (!self || !g_current_task_ptr) {
        return;
    }
    g_current_task_ptr->wake_tick = wake_tick;
    g_current_task_ptr->sleeping = true;
    JLOS_TASK_SET_BLOCKED(g_current_task_ptr);
    jlos_list_head_t *pos;
    jlos_list_for_each(pos, &self->sleep_queue) {
        jlos_task_t *t = container_of(pos, jlos_task_t, wait_node);
        if (wake_tick < t->wake_tick) {
            break;
        }
    }
    jlos_list_add_tail(&g_current_task_ptr->wait_node, pos);
}

const char *jlos_task_status_map_str(uint32_t status)
{
    switch (status) {
        case JLOS_TASK_READY :
            return "ready";
        case JLOS_TASK_RUNNING :
            return "running";
        case JLOS_TASK_BLOCKED :
            return "blocked";
        case JLOS_TASK_ZOMBIE :
            return "zombie";
        case JLOS_TASK_WAITING :
            return "waiting";
        default:
            break;
    }
    return "error";
}

static uint32_t jlos_exec_setup_user_stack(jlos_task_t *task, int argc, char *const argv[], char *const envp[])
{
    uint32_t stack_base = JLOS_TASK_USER_STACK_TOP - JLOS_TASK_USER_STACK_SIZE;
    for (uint32_t addr = stack_base; addr <= JLOS_TASK_USER_STACK_TOP; addr += JLOS_PAGE_FRAME_SIZE) {
        void *frame = jlos_page_frame_malloc();
        if (!frame) {
            return 0;
        }
        if (!jlos_paging_map(task->mm->pc, addr, VIRT_TO_PHYS(frame), JLOS_PTE_USER_RW)) {
            jlos_page_frame_free(frame);
            return 0;
        }
    }
    jlos_paging_switch(task->mm->pc);
    uint32_t sp = JLOS_TASK_USER_STACK_TOP;
    uint32_t envp_addrs[JLOS_EXECVE_MAX_ARGS];
    int envc = 0;
    if (envp) {
        for (int i = 0; envp[i] && i < JLOS_EXECVE_MAX_ARGS; i++) {
            size_t len = jlos_strlen(envp[i]) + 1;
            sp -= len;
            jlos_memcpy((void *)sp, envp[i], len);
            envp_addrs[i] = sp;
            envc = i + 1;
        }
    }
    uint32_t argv_addr[JLOS_EXECVE_MAX_ARGS];
    int eff_argc = 0;
    if (argv) {
        for (int i = 0; i < argc && i < JLOS_EXECVE_MAX_ARGS; i++) {
            size_t len = jlos_strlen(argv[i]) + 1;
            sp -= len;
            jlos_memcpy((void *)sp, argv[i], len);
            argv_addr[i] = sp;
            eff_argc = i + 1;
        }
    }
    uint32_t ptr_size = sizeof(uint32_t);
    sp &= ~(ptr_size - 1);
    sp -= ptr_size;
    *(uint32_t *)sp = 0;

    for (int i = envc - 1; i >= 0; i--) {
        sp -= ptr_size;
        *(uint32_t *)sp = envp_addrs[i];
    }
    sp -= ptr_size;
    *(uint32_t *)sp = 0;
    for (int i = eff_argc - 1; i >= 0; i--) {
        sp -= ptr_size;
        *(uint32_t *)sp = argv_addr[i];
    }
    sp -= ptr_size;
    *(int *)sp = eff_argc;

    return sp;
}

int jlos_process_exec_elf(jlos_task_t *task, const char *path, int argc, char *const argv[], char *const envp[])
{
    jlos_vfs_file_t *file = jlos_vfs_open(path, JLOS_VFS_O_RDONLY);
    if (!file) {
        printk_err("exec_elf: failed to open %s\n", path);
        return -1;
    }
    if (g_task_manager_ptr) {
        jlos_hash_chain_remove(&g_task_manager_ptr->pid_hash, &task->pid_hash_node);
    }
    if (task->mm) {
        jlos_mm_destroy(task->mm);
        task->mm = NULL;
    }
    jlos_task_unmap_user_stack(task);
    if (!task->stack) {
        task->stack = (uint8_t *)jlos_kalloc(JLOS_TASK_STACK_SIZE);
        if (!task->stack) {
            jlos_vfs_close(file);
            jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
            return -1;
        }
        task->stack_size = JLOS_TASK_STACK_SIZE;
        jlos_memset(task->stack, 0, JLOS_TASK_STACK_SIZE);
    }
    
    task->mm = jlos_mm_create();
    if (!task->mm) {
        jlos_vfs_close(file);
        jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
        return -1;
    }
    
    jlos_paging_context_clone(task->mm->pc, &s_kernel_paging_context);

    uint32_t entry;
    if (!jlos_elf_load(task->mm, file, &entry)) {
        printk_err("exec_elf: elf_load failed\n");
        jlos_vfs_close(file);
        jlos_mm_destroy(task->mm);
        task->mm = NULL;
        jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
        return -1;
    }
    jlos_vfs_close(file);

    jlos_rbtree_node_t *last = jlos_rbtree_last(&task->mm->vma_tree);
    if (last) {
        jlos_vma_t *vma = jlos_rbtree_entry(last, jlos_vma_t, rb_node);
        task->mm->brk_start = JLOS_PAGE_ALIGN_UP(vma->end);
        task->mm->brk_end = task->mm->brk_start;
        task->mm->brk_limit = JLOS_TASK_USER_BRK_LIMIT;
    }

    jlos_task_create_user_stack(task);
    uint32_t user_stack_top = jlos_exec_setup_user_stack(task, argc, argv, envp);
    if (user_stack_top == 0) {
        jlos_process_exit(task, TASK_EXIT_PAGE_FAULT);
        return -1;
    }
    task->is_user_process = true;
    if (!task->fds) {
        task->fds = (jlos_task_fd_t *)jlos_kalloc(sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
        if (task->fds) {
            for (uint32_t i = 0; i < 3 && i < JLOS_TASK_FDS_NUM; i++) {
                task->fds[i].type = JLOS_TASK_FD_CONSOLE;
                task->fds[i].obj = NULL;
                task->fds[i].flags = i ? JLOS_TASK_FD_WRITE_ONLY : JLOS_TASK_FD_READ_ONLY;
            }
            for (uint32_t i = 3; i < JLOS_TASK_FDS_NUM; i++) {
                task->fds[i].type = JLOS_TASK_FD_UNUSED;
                task->fds[i].obj = NULL;
                task->fds[i].flags = 0;
            }
        }
    }
    jlos_mmu_t *mmu = jlos_mmu_get_kernel();
    jlos_cpu_state_init(&task->cpustate);
    jlos_arch_task_init_arch_user(&task->cpustate,
        mmu, (void(*)(void))entry, task->stack, task->stack_size, user_stack_top);
    
    if (g_task_manager_ptr) {
        jlos_hash_chain_insert(&g_task_manager_ptr->pid_hash, &task->pid, &task->pid_hash_node);
    }
    if (g_hal_syscall_trapframe) {
        jlos_paging_switch(task->mm->pc);
        jlos_cpu_state_set_user_entry(g_hal_syscall_trapframe, entry, user_stack_top);
        return 0;
    }
    jlos_arch_exec_return(task->mm->pc, entry, user_stack_top);
    return 0;
}

JLOS_INITCALL(JLOS_INITCALL_CORE, jlos_task_manager_init);
