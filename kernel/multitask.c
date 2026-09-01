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

extern jlos_task_t *g_current_task_ptr;
extern jlos_paging_context_t s_kernel_paging_context;
extern uint32_t _kernel_end;   /* linker 符号: 内核 image 结束虚拟地址 (multitask.h 已声明, 此处保留供本 TU 早期引用) */

jlos_task_manager_t *g_task_manager_ptr = NULL;

static uint32_t s_next_pid = 1;

static inline uint32_t pid_hash_fn(uint32_t pid)
{
    return pid % jLOS_TASK_PID_HASH_SIZE;
}

static void pid_hash_insert(jlos_task_manager_t *self, jlos_task_t *task)
{
    uint32_t b = pid_hash_fn(task->pid);
    task->next_hash = self->pid_hash[b];
    self->pid_hash[b] = task;
}

static void pid_hash_remove(jlos_task_manager_t *self, jlos_task_t *task)
{
    uint32_t b = pid_hash_fn(task->pid);
    jlos_task_t **pp = &self->pid_hash[b];
    while (*pp) {
        if (*pp == task) {
            *pp = task->next_hash;
            task->next_hash = NULL;
            return;
        }
        pp = &(*pp)->next_hash;
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
    t->yield = true;
}

/* 进程退出 → ZOMBIE: 保留 PCB 等待父进程通过 wait_pid 收割 */
void jlos_task_set_zombie(jlos_task_t *t, uint32_t exit_code)
{
    if (!t) {
        return;
    }
    t->status = JLOS_TASK_ZOMBIE;
    t->exit_code = exit_code;
}

void jlos_task_set_waiting(jlos_task_t *t, uint32_t pid)
{
    if (!t) {
        return;
    }
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
    self->next_hash = NULL;
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
    self->fds_size = 0;
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
            printk("[ustack] pid=%u frame alloc FAILED\n", self->pid);
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
        self->fds_size = JLOS_TASK_FDS_NUM;
    } else {
        self->fds_size = 0;
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
    pid_hash_remove(self, task);
    jlos_arch_task_ext_destroy(task);
    if (task->stack) {
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
    for (int i = 0; i < self->num_tasks; i++) {
        if (self->tasks[i] == task) {
            self->tasks[i] = self->tasks[self->num_tasks - 1];
            self->tasks[self->num_tasks - 1] = NULL;
            self->num_tasks--;
            break;
        }
    }
    if (g_current_task_ptr == task) {
        g_current_task_ptr = NULL;
    }
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
    self->num_tasks = 0;
    self->current_task = -1;
    self->main_thread_saved = false;
    for (int i = 0; i < jLOS_TASK_PID_HASH_SIZE; i++) {
        self->pid_hash[i] = NULL;
    }
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

jlos_task_t *jlos_task_manager_find_pid(jlos_task_manager_t *self, uint32_t pid)
{
    uint32_t b = pid_hash_fn(pid);
    for (jlos_task_t *t = self->pid_hash[b]; t; t = t->next_hash) {
        if (t->pid == pid) {
            return t;
        }
    }
    return NULL;
}

bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task)
{
    if (self->num_tasks >= JLOS_TASK_MAX_NUM) {
        return false;
    }
    self->tasks[self->num_tasks] = task;
    self->num_tasks++;
    pid_hash_insert(self, task);
    return true;
}

jlos_cpu_state_t *jlos_task_manager_schedule(jlos_task_manager_t* self, jlos_cpu_state_t *cpustate)
{
    if (self == NULL || cpustate == NULL) return cpustate;

    /* 经典 Linux task_struct.thread.sp 显式存取: 入口把 cpustate (= on-stack
     * pt_regs 基指针, 100% ∈ [stack, +size)) 存进 current->sp.value, fork 子栈
     * 平移 delta 唯一参考此 sp 值, 不用 cpustate (heap 成员) + N 派生物. */
    if (self->current_task >= 0 && self->current_task < (int)self->num_tasks) {
        jlos_task_t *curr = self->tasks[self->current_task];
        if (curr) {
            jlos_arch_task_set_sp(curr, (uint32_t)cpustate);
        }
    }

    if (self->num_tasks <= 0) {
        g_current_task_ptr = NULL;
        return cpustate;
    }
    /* 仅回收孤儿 ZOMBIE：无父、或父也已 ZOMBIE。
     * 父存活的 ZOMBIE 必须保留给父 wait/reap，否则父 find_pid 收割不到 exit_code。 */
    for (int i = self->num_tasks - 1; i >= 0; i--) {
        jlos_task_t *t = self->tasks[i];
        if (!t || t->status != JLOS_TASK_ZOMBIE) {
            continue;
        }
        if (t == g_current_task_ptr) {
            g_current_task_ptr = NULL;
            continue;
        }
        jlos_task_t *parent = jlos_task_manager_find_pid(self, t->parent_pid);
        if (!parent || parent->status == JLOS_TASK_ZOMBIE) {
            jlos_task_free(self, t);
        }
    }
    if (self->current_task >= self->num_tasks) {
        self->current_task = -1;
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
        if (current) {
            /* 无论 status 是什么(RUNNING/WAITING/READY/...), 都保存 cpustate.
             * 原代码只在 RUNNING 时 save — 若 task 设 WAITING 后被抢占,
             * cpustate 不更新 → 下次恢复时 eip/eflags 是旧垃圾值 → ip=0x6/0x38. */
            if (current->status == JLOS_TASK_RUNNING) {
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
            jlos_task_t *child = jlos_task_manager_find_pid(self, t->waiting_pid);
            if (child && child->status == JLOS_TASK_ZOMBIE) {
                t->waiting_pid = t->pid;
                JLOS_TASK_SET_READY(t);
                t->last_ready_tick = now_tick;
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
                jlos_paging_context_t *target_mm = next->mm ? next->mm : &s_kernel_paging_context;
                jlos_paging_switch(target_mm);
                JLOS_TASK_SET_RUNNING(next);
                g_current_task_ptr = next;
                jlos_arch_tss_set_ctx((uint32_t)(next->stack + next->stack_size));
                jlos_arch_task_ext_switch();
                return &next->cpustate;
            }
        }
    }
    /* 没有Ready任务，恢复主线程的状态 */
    g_current_task_ptr = NULL;
    self->current_task = -1;
    jlos_paging_switch(&s_kernel_paging_context);
    jlos_arch_task_ext_switch();
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

jlos_task_t *jlos_process_fork(jlos_task_manager_t *self, jlos_task_t *parent,
                               uint32_t fork_return_pc,
                               uint32_t fork_esp_ref,
                               uint32_t fork_ebp_ref,
                               uint32_t fork_resume_pc)
{
    /* 经典 Linux copy_thread: jlos_arch_fork_invoke asm 首句读硬件 esp/ebp 作
     * 锚点, HAL prepare_child 沿帧链或 K_DEAD=-24 硬编码求子栈 iret_sp.
     * 帧链法精确但 -fomit-frame-pointer 时不命中, K_DEAD 0 方差兜底. */

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
        printk("[fk-OV] task=%p (sz=%zu) below writable min; rollback OOM\n",
               child, sizeof(jlos_task_t));
        jlos_kfree(child);
        return NULL;
    }
    *child = *parent;
    child->stack = (uint8_t *)jlos_kalloc(JLOS_TASK_STACK_SIZE);
    child->stack_size = JLOS_TASK_STACK_SIZE;
    if (child->stack) {
        if (!JLOS_KERN_PTR_VALID(child->stack, JLOS_TASK_STACK_SIZE)) {
            printk("[fk-OV] stack=%p (sz=%u) below writable min; rollback OOM\n",
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
    
    if (parent->fds) {
        child->fds = (jlos_task_fd_t *)jlos_kalloc(sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
        if (child->fds) {
            jlos_memcpy(child->fds, parent->fds, sizeof(jlos_task_fd_t) * JLOS_TASK_FDS_NUM);
            child->fds_size = JLOS_TASK_FDS_NUM;
            for (int i = 0; i < JLOS_TASK_FDS_NUM; i++) {
                if (child->fds[i].type == JLOS_TASK_FD_PIPE && child->fds[i].obj) {
                    ((jlos_pipe_t *)child->fds[i].obj)->refcount++;
                }
            }
        } else {
            child->fds = NULL;
            child->fds_size = 0;
        }
    } else {
        child->fds = NULL;
        child->fds_size = 0;
    }

    /* 分配失败/越界统一ROLLBACK: 绝不把半截task struct挂进调度器 */
    if (child->stack == NULL) {
        goto ROLLBACK_OOM;
    }

    /* 仅 user process 子任务需要独立 mm(克隆内核上下文+USER段映射);
     * kernel task 子任务保持 mm=NULL, 复用 s_kernel_paging_context —
     * 否则克隆页目录会浅拷贝共享内核页表, change_flags 修改共享页表
     * 污染全局, 且调度器切到子克隆 CR3 后堆扩展PDE缺失 → buddy元数据破坏 */
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

    child->next_hash = NULL;
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
    task->pid = s_next_pid++;
    task->parent_pid = 0;
    task->exit_code = TASK_EXIT_DEAUFT;
    return 0;
}

void jlos_process_exit(jlos_task_t *task, uint32_t exit_code)
{
    /* 兜底: task≠curr时exit curr, 永远不误杀非当前task(exit永不返回caller) */
    jlos_task_t *real = g_current_task_ptr;
    if (!task || (real && task != real)) {
        if (real && task != real) {
            printk("[ex-FIX] caller passed task=%p(pid=%u) but curr=%p(pid=%u); "
                   "exit curr instead\n",
                   task, task? (unsigned)task->pid : 0u,
                   real, (unsigned)real->pid);
        }
        if (!real) {
            jlos_task_manager_t *mgr = g_task_manager_ptr;
            if (mgr) real = jlos_task_manager_curr_task_on_tick(mgr);
            if (!real) { for (;;) jlos_hal_halt(); }
        }
        task = real;
    }
    JLOS_TASK_SET_ZOMBIE(task, exit_code);
    task->yield = true;
    for (;;) {
        jlos_hal_halt();
    }
}
