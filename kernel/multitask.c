#include <hal/context.h>
#include <hal/timer.h>
#include <hal/cpu_state.h>
#include <hal/hal.h>          /* jlos_hal_halt */
#include <kernel/multitask.h>
#include <kernel/memory_manager.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/paging.h>
#include <kernel/printk.h>
#include <kernel/ipc.h>

#define JLOS_KERNEL_LOG_SUBSYS "sched"

extern jlos_task_t *g_current_task_ptr;
extern jlos_paging_context_t s_kernel_paging_context;
extern uint32_t _kernel_end;

jlos_task_manager_t *g_task_manager_ptr = NULL;

static uint32_t s_next_pid = 1;

static inline bool task_on_rq(jlos_task_t *t)
{
    return t->rq_node.next && t->rq_node.next != &t->rq_node;
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
    int l = __builtin_ctz(self->rq_nonempty);
    jlos_list_head_t *first = self->rq[l].head.next;
    jlos_list_del(first);
    if (--self->rq[l].count == 0) {
        self->rq_nonempty &= ~(1U << l);
    }
    jlos_task_t *t = container_of(first, jlos_task_t, rq_node);
    uint32_t now = jlos_hal_timer_get_ticks();
    while(l > 0 && (now - t->last_ready_tick) > JLOS_TASK_MLFQ_AGING_TICKS) {
        t->priority = (uint32_t)(--l);
        t->default_slice = (2U << l);
        t->last_ready_tick = now;
    }
    return t;
}

static void rq_remove_specific(jlos_task_manager_t *self, jlos_task_t *t)
{
    if (!t || !task_on_rq(t)) {
        return;
    }
    uint32_t l = clamp_pri(t->priority);
    jlos_list_del(&t->rq_node);
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
    self->pid_hash_node.next = NULL;
    self->pid_hash_node.pprev = NULL;
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
    self->brk_start = 0;
    self->brk_end = 0;
    self->brk_limit = 0;
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
    self->mm = (jlos_paging_context_t *)jlos_kalloc(sizeof(jlos_paging_context_t));
    if (!self->mm) {
        return -TASK_ERR_NOMEM;
    }
    jlos_paging_context_clone(self->mm, &s_kernel_paging_context);

    const jlos_hal_kernel_segments_t *seg = jlos_hal_get_kernel_segments();
    if (seg) {
        if (seg->text_start != 0) {
            jlos_paging_change_flags_range(self->mm,
                JLOS_PAGE_ALIGN_DOWN(seg->text_start),
                JLOS_PAGE_ALIGN_DOWN(seg->text_end + JLOS_PAGE_SIZE - 1), JLOS_PTE_USER_RO);
        }
        if (seg->rodata_start != 0) {
            jlos_paging_change_flags_range(self->mm,
                JLOS_PAGE_ALIGN_DOWN(seg->rodata_start),
                JLOS_PAGE_ALIGN_DOWN(seg->rodata_end + JLOS_PAGE_SIZE - 1), JLOS_PTE_USER_RO);
        }
        if (seg->data_start != 0) {
            jlos_paging_change_flags_range(self->mm,
                JLOS_PAGE_ALIGN_DOWN(seg->data_start),
                JLOS_PAGE_ALIGN_DOWN(seg->data_end + JLOS_PAGE_SIZE - 1), JLOS_PTE_USER_RW);
        }
        if (seg->bss_start != 0) {
            jlos_paging_change_flags_range(self->mm,
                JLOS_PAGE_ALIGN_DOWN(seg->bss_start),
                JLOS_PAGE_ALIGN_DOWN(seg->bss_end + JLOS_PAGE_SIZE - 1), JLOS_PTE_USER_RW);
        }
    }
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
    {
        void *frame = jlos_page_frame_malloc();
        if (frame) {
            jlos_memset(frame, 0, JLOS_PAGE_FRAME_SIZE);
            jlos_paging_map(self->mm, stack_base, VIRT_TO_PHYS(frame), JLOS_PTE_USER_RW);
            /* 仅映射栈底1页; user_esp=栈顶, 首次push依赖#PF demand-map自愈 */
            (void)self;
        } else {
            printk_err("[ustack] pid=%u frame alloc FAILED\n", self->pid);
        }
    }
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
        for (int i = 0; i < JLOS_TASK_FDS_NUM; i++) {
            self->fds[i].type = JLOS_TASK_FD_UNUSED;
            self->fds[i].obj = NULL;
            self->fds[i].flags = 0;
        }
    }
    for (uint32_t i = 0; i < 3 && i < JLOS_TASK_FDS_NUM; i++) {
        self->fds[i].type = JLOS_TASK_FD_CONSOLE;
        self->fds[i].obj = NULL;
        self->fds[i].flags = i ? JLOS_TASK_FD_WRITE_ONLY : JLOS_TASK_FD_READ_ONLY;
    }
    self->brk_start = JLOS_TASK_USER_BRK_START;
    self->brk_end = self->brk_start;
    self->brk_limit = JLOS_TASK_USER_BRK_LIMIT;
    self->is_user_process = true;
    jlos_memset(self->stack, 0, JLOS_TASK_STACK_SIZE);

    int32_t ret = jlos_task_create_user_mm(self);
    if (ret < 0) {
        if (self->mm) {
            jlos_paging_context_destroy(self->mm);
            jlos_kfree(self->mm);
        }
        jlos_kfree(self->fds);
        jlos_kfree(self->stack);
        return ret;
    }
    ret = jlos_task_create_user_stack(self);
    if (ret < 0) {
        jlos_paging_context_destroy(self->mm);
        jlos_kfree(self->mm);
        jlos_kfree(self->fds);
        jlos_kfree(self->stack);
        return ret;
    }
    jlos_cpu_state_init(&self->cpustate);
    jlos_arch_task_init_arch_user(&self->cpustate, mmu, entrypoint,
        self->stack, self->stack_size, JLOS_TASK_USER_STACK_TOP, 0x2B);
    return 0;
}

static void jlos_task_unmap_user_stack(jlos_task_t *task)
{
    if (!task->user_stack) {
        return;
    }
    uint32_t base = JLOS_PAGE_ALIGN_DOWN((uint32_t)task->user_stack);
    uint32_t top = base + task->user_stack_size;
    jlos_paging_context_t *mm = task->mm ? task->mm : jlos_active_paging_context;
    for (uint32_t addr = base; addr < top; addr += JLOS_PAGE_FRAME_SIZE) {
        jlos_paging_unmap(mm, addr);
    }
    task->user_stack = NULL;
}

void jlos_task_free(jlos_task_manager_t *self, jlos_task_t *task)
{
    if (!self || !task) {
        return;
    }
    jlos_hash_chain_remove(&self->pid_hash, &task->pid_hash_node);
    if (task->zombie_node.next && task->zombie_node.next != &task->zombie_node) {
        jlos_list_del(&task->zombie_node);
    }
    jlos_arch_task_ext_destroy(task);
    if (task->stack && task->pid != 0) {
        jlos_kfree(task->stack);
        task->stack = NULL;
    }
    if (task->mm) {
        jlos_paging_context_destroy(task->mm);
        jlos_kfree(task->mm);
        task->mm = NULL;
    } else {
        jlos_task_unmap_user_stack(task);
    }
    if (task->fds) {
        jlos_kfree(task->fds);
        task->fds = NULL;
    }
    int32_t idx = task->slot_idx;
    if (idx >= 0 && idx < self->num_tasks && self->tasks[idx] == task) {
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

void jlos_task_manager_init(jlos_task_manager_t* self)
{
    jlos_hash_chain_init(&self->pid_hash, jLOS_TASK_PID_HASH_SIZE, jlos_hash_uint32, pid_hash_cmp);
    for (uint32_t l = 0; l < JLOS_TASK_MLFQ_LEVELS; l++) {
        jlos_list_init(&self->rq[l].head);
        self->rq[l].count = 0;
    }
    jlos_list_init(&self->zombie_head);
    self->need_resched = false;
    self->rq_nonempty = 0;
    self->idle_task = (jlos_task_t *)jlos_kalloc(sizeof(jlos_task_t));
    if (!self->idle_task) {
        for (;;) {
            jlos_hal_halt();
        }
    }
    jlos_task_init_1(self->idle_task, "idle");
    self->idle_task->pid = 0;
    self->idle_task->waiting_pid = 0;
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
    g_task_manager_ptr = self;
}

void jlos_task_manager_destroy(jlos_task_manager_t* self)
{
    while (self->num_tasks) {
        jlos_task_free(self, self->tasks[self->num_tasks - 1]);
    }
    jlos_hash_chain_destroy(&self->pid_hash);
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
    if (self->num_tasks >= JLOS_TASK_MAX_NUM) {
        return false;
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

jlos_cpu_state_t *jlos_task_manager_schedule(jlos_task_manager_t* self, jlos_cpu_state_t *cpustate)
{
    if (!self) {
        return cpustate;
    }
    jlos_task_t *prev = NULL;
    if (self->current_task < (int)self->num_tasks) {
        prev = self->tasks[self->current_task];
        if (prev) {
            jlos_arch_task_set_sp(prev, (uint32_t)cpustate);
        }
    }
    if (prev) {
        jlos_memcpy(&prev->cpustate, cpustate, sizeof(jlos_cpu_state_t));
        if (!jlos_cpu_state_is_user_mode(cpustate)) {
            jlos_cpu_state_record_user_stack(&prev->cpustate, cpustate);
        }
        if (prev->status == JLOS_TASK_RUNNING) {
            if (prev->sleeping) {
                JLOS_TASK_SET_BLOCKED(prev);
            } else if (prev->yield || !prev->remain_slice) {
                if (!prev->remain_slice && prev->priority < JLOS_TASK_MLFQ_LEVELS - 1) {
                    prev->priority++;
                    prev->default_slice = (2U << prev->priority);
                }
                JLOS_TASK_SET_READY(prev);
                rq_enqueue(self, prev);
            } else {
                JLOS_TASK_SET_READY(prev);
                rq_enqueue(self, prev);
            }
        }
    }

    if (!jlos_list_empty(&self->zombie_head)) {
        jlos_list_head_t *pos, *n;
        jlos_list_for_each_safe(pos, n, &self->zombie_head) {
            jlos_task_t *z = container_of(pos, jlos_task_t, zombie_node);
            if (z == prev) continue;
            jlos_list_del_init(pos);
            jlos_task_free(self, z);
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
            if (t->status == JLOS_TASK_BLOCKED) {
                JLOS_TASK_SET_READY(t);
                if (!task_on_rq(t)) {
                    rq_enqueue(self, t);
                }
            }
        }
        if (t->status == JLOS_TASK_WAITING && t->waiting_pid != t->pid) {
            jlos_task_t *c = jlos_task_manager_find_pid(self, t->waiting_pid);
            if (c && c->status == JLOS_TASK_ZOMBIE) {
                t->waiting_pid = t->pid;
                JLOS_TASK_SET_READY(t);
                if (!task_on_rq(t)) {
                    rq_enqueue(self, t);
                }
            }
        }
    }

    jlos_task_t *next = rq_dequeue(self);
    if (!next) {
        next = self->idle_task;
    }

    jlos_paging_context_t *target_mm = next->mm ? next->mm : &s_kernel_paging_context;
    jlos_paging_switch(target_mm);
    
    int idx = next->slot_idx;
    if (idx < 0 || idx >= self->num_tasks || self->tasks[idx] != next) {
        idx = -1;
        for (int i = 0; i < self->num_tasks; i++) {
            if (self->tasks[i] == next) {
                idx = i;
                next->slot_idx = i;
                break;
            }
        }
    }
    if (idx < 0) {
        printk_err("[sched] next=%p not in tasks[], fallback failed\n", next);
        return cpustate;
    }
    self->current_task = idx;
    g_current_task_ptr = next;
    JLOS_TASK_SET_RUNNING(next);
    jlos_arch_tss_set_ctx((uint32_t)(next->stack + next->stack_size));
    jlos_arch_task_ext_switch();
    self->need_resched = false;
    return &next->cpustate;
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

jlos_task_t *jlos_process_fork(jlos_task_manager_t *self, jlos_task_t *parent,
                               uint32_t fork_return_pc,
                               uint32_t fork_esp_ref,
                               uint32_t fork_ebp_ref,
                               uint32_t fork_resume_pc)
{
    if (self->num_tasks >= JLOS_TASK_MAX_NUM) {
        return NULL;
    }

    jlos_task_t *child = (jlos_task_t *)jlos_kalloc(sizeof(jlos_task_t));
    if (!child) {
        return NULL;
    }
    /* 范围校验: kalloc 返回值必须 >= _kernel_end (内核 image 之后皆为可写区).
     * [fk-OV] 必须保留: kalloc 返回内核 .text 段时仅有的定位证据. */
    if (!JLOS_KERN_PTR_VALID(child, sizeof(jlos_task_t))) {
        printk_err("[fk-OV] task=%p (sz=%u) below writable min; rollback OOM\n",
               child, sizeof(jlos_task_t));
        jlos_kfree(child);
        return NULL;
    }
    *child = *parent;
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
    child->remain_slice = parent->default_slice;
    child->next_wait = NULL;
    child->pid_hash_node.next = NULL;
    child->pid_hash_node.pprev = NULL;
    jlos_list_init(&child->rq_node);
    jlos_list_init(&child->zombie_node);
    
    if (parent->fds) {
        child->fds = (jlos_task_fd_t *)jlos_kalloc(sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
        if (child->fds) {
            jlos_memcpy(child->fds, parent->fds, sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
            for (int i = 0; i < JLOS_TASK_FDS_NUM; i++) {
                if (child->fds[i].type == JLOS_TASK_FD_PIPE && child->fds[i].obj) {
                    ((jlos_pipe_t *)child->fds[i].obj)->refcount++;
                }
            }
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
        for (uint32_t pd_idx = 0; pd_idx < (KERNEL_VIRTUAL_BASE >> 22); pd_idx++) {
            jlos_page_dir_entry_t *pde = &parent->mm->page_dir->entries[pd_idx];
            if (!(*pde & JLOS_PDE_PRESENT)) {
                continue;
            }
            jlos_page_table_t *pt = (jlos_page_table_t *)PHYS_TO_VIRT(*pde & JLOS_PAGE_ADDR_MASK);
            for (uint32_t pt_idx = 0; pt_idx < JLOS_PAGE_TABLE_ENTRIES; pt_idx++) {
                jlos_page_table_entry_t *pte = &pt->entries[pt_idx];
                if (!(*pte & JLOS_PTE_PRESENT)) {
                    continue;
                }
                uint32_t phys = *pte & JLOS_PAGE_ADDR_MASK;
                uint32_t vir = (pd_idx << 22) | (pt_idx << 12);
                jlos_page_frame_refcount_inc(phys);
                jlos_paging_map(child->mm, vir, phys, JLOS_PTE_USER_COW);
            }
        }
        jlos_paging_change_flags_range(parent->mm, 0, KERNEL_VIRTUAL_BASE, JLOS_PTE_USER_COW);
    }

    child->exit_code = TASK_EXIT_DEAUFT;
    /* HAL prepare_child: 子cpustate on-kstack 56B + epilogue 5slot; fork_return_pc必须来自fork_stub.c(本层取=错frame) */
    jlos_arch_task_fork_prepare_child(
        &child->cpustate,
        child->is_user_process,
        parent->stack, parent->stack_size,
        child->stack,  child->stack_size,
        &parent->cpustate,
        fork_return_pc,
        fork_esp_ref,
        fork_ebp_ref,
        fork_resume_pc,
        parent, child);
    if (!jlos_task_manager_add_task(self, child)) {
        jlos_kfree(child->stack);
        jlos_kfree(child->fds);
        if (child->mm) {
            jlos_paging_context_destroy(child->mm);
            jlos_kfree(child->mm);
        }
        jlos_kfree(child);
        return NULL;
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
        jlos_paging_context_destroy(task->mm);
        jlos_kfree(task->mm);
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
    task->exit_code = TASK_EXIT_DEAUFT;
    return 0;
}

void jlos_process_exit(jlos_task_t *task, uint32_t exit_code)
{
    (void)task;
    if (!g_current_task_ptr || !g_task_manager_ptr) {
        goto halt;
    }
    JLOS_TASK_SET_ZOMBIE(g_current_task_ptr, exit_code);
    jlos_sched_wake_waiter(g_task_manager_ptr, g_current_task_ptr->pid);
    bool has_alive_parent = false;
    if (g_current_task_ptr->parent_pid && g_current_task_ptr->parent_pid != g_current_task_ptr->pid) {
        jlos_task_t *parent = jlos_task_manager_find_pid(g_task_manager_ptr, g_current_task_ptr->parent_pid);
        if (parent && parent->status != JLOS_TASK_ZOMBIE) {
            has_alive_parent = true;
        }
    }
    if (!has_alive_parent) {
        jlos_list_add(&g_current_task_ptr->zombie_node, &g_task_manager_ptr->zombie_head);
    }
    g_task_manager_ptr->need_resched = true;
halt:
    jlos_hal_enable_interrupts();
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
    for (int i = 0; i < self->num_tasks; i++) {
        jlos_task_t *p = self->tasks[i];
        if (!p) continue;
        if (p->status == JLOS_TASK_WAITING && p->waiting_pid == exited_pid) {
            p->waiting_pid = p->pid;
            JLOS_TASK_SET_READY(p);
            if (!task_on_rq(p)) {
                rq_enqueue(self, p);
            }
            self->need_resched = true;
            break;
        }
    }
}
