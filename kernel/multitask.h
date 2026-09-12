#ifndef _JLOS__KERNEL_MULTITASK_H
#define _JLOS__KERNEL_MULTITASK_H

#include <common/types.h>
#include <hal/mmu.h>
#include <hal/cpu_state.h>
#include <hal/ext_state.h>
#include <dsa/list.h>
#include <dsa/hash_chain.h>
#include <kernel/vma.h>

#define JLOS_TASK_READY             0
#define JLOS_TASK_RUNNING           1
#define JLOS_TASK_BLOCKED           2
#define JLOS_TASK_WAITING           4
#define JLOS_TASK_ZOMBIE            5

#define JLOS_TASK_STACK_SIZE        16384
#define JLOS_TASK_NAME_SIZE         32

#define JLOS_TASK_MLFQ_LEVELS       4
#define JLOS_TASK_MLFQ_AGING_TICKS  200

#define JLOS_TASK_FDS_NUM           16
#define JLOS_TASK_MAX_NUM           256

#define JLOS_TASK_FD_READ_ONLY      1
#define JLOS_TASK_FD_WRITE_ONLY     2
#define JLOS_TASK_FD_READ_WRITE     3

#define JLOS_TASK_USER_BRK_START    0x08000000
#define JLOS_TASK_USER_BRK_SIZE     0x01000000
#define JLOS_TASK_USER_BRK_LIMIT    (JLOS_TASK_USER_BRK_START + JLOS_TASK_USER_BRK_SIZE)

#define JLOS_TASK_USER_STACK_TOP    0xBFFFF000
#define JLOS_TASK_USER_STACK_SIZE   0x00010000

extern uint32_t _kernel_end;
#define JLOS_KERN_WRITABLE_MIN   ((uint32_t)(unsigned long)&_kernel_end)
#define JLOS_KERN_U32OF(p)       ((uint32_t)(unsigned long)(p))
#define JLOS_KERN_PTR_VALID(p,sz)  ( (JLOS_KERN_U32OF(p) >= JLOS_KERN_WRITABLE_MIN) && \
                                     (JLOS_KERN_U32OF(p) + (sz) >  JLOS_KERN_U32OF(p)) && \
                                     (JLOS_KERN_U32OF(p) + (sz) <= 0xFFFFFFFFu) )

#define jLOS_TASK_PID_HASH_SIZE     256

typedef enum {
    TASK_EXIT_DEAUFT        = 0,
    TASK_EXIT_PAGE_FAULT
} jlos_task_exit_code;

enum jlos_task_errno {
    TASK_ERR_NOMEM      = 1,
    TASK_ERR_INVAL      = 2,
    TASK_ERR_NOT_FOUND  = 3,
    TASK_ERR_NO_CHILD   = 4,
};

typedef enum {
    JLOS_TASK_FD_UNUSED = 0,
    JLOS_TASK_FD_PIPE   ,
    JLOS_TASK_FD_CONSOLE,
    JLOS_TASK_FD_FILE
} jlos_task_fd_type_t;

typedef struct {
    jlos_task_fd_type_t type;
    void *obj;
    uint8_t flags;
} jlos_task_fd_t;

typedef struct jlos_paging_context jlos_paging_context_t;

typedef struct jlos_task {
    volatile uint32_t       status;
    char                    name[JLOS_TASK_NAME_SIZE];
    uint8_t                 *stack;
    uint32_t                stack_size;
    uint8_t                 *user_stack;
    uint32_t                user_stack_size;
    jlos_cpu_state_t        cpustate;
    jlos_arch_sp_t          sp;
    uint32_t                pid;
    uint32_t                parent_pid;
    jlos_task_exit_code     exit_code;
    bool                    is_user_process;
    jlos_mm_t               *mm;
    uint32_t                wake_tick;
    bool                    sleeping;
    bool                    yield;
    int32_t                 errno;
    uint32_t                waiting_pid;
    uint32_t                priority;
    uint32_t                remain_slice;
    uint32_t                default_slice;
    uint32_t                last_ready_tick;
    jlos_task_fd_t          *fds;
    uint32_t                brk_start;
    uint32_t                brk_end;
    uint32_t                brk_limit;
    jlos_list_head_t        wait_node;
    jlos_hash_node_t        pid_hash_node;
    jlos_list_head_t        zombie_node;
    jlos_list_head_t        rq_node;
    int32_t                 slot_idx;
    jlos_arch_ext_state_t   ext_state;
} __attribute__((aligned(JLOS_ARCH_EXT_STATE_ALIGN))) jlos_task_t;

typedef struct {
    jlos_task_t         *tasks[JLOS_TASK_MAX_NUM];
    jlos_hash_chain_t   pid_hash;
    int                 num_tasks;
    int                 current_task;
    jlos_list_head_t    zombie_head;
    jlos_list_head_t    sleep_queue;
    struct {
        jlos_list_head_t head;
        uint32_t count;
    }                   rq[JLOS_TASK_MLFQ_LEVELS];
    uint32_t            rq_nonempty;
    jlos_task_t         *idle_task;
    bool                need_resched;
} jlos_task_manager_t;

#define jlos_task_curr()  (g_current_task_ptr)

extern jlos_task_t *g_current_task_ptr;

int32_t jlos_task_init(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name);
int32_t jlos_task_init_user(jlos_task_t* self, jlos_mmu_t *mmu, void (*entrypoint)(void), const char *name);
void jlos_task_free(jlos_task_manager_t *self, jlos_task_t *task);

int32_t jlos_task_fd_malloc(jlos_task_t *task);
jlos_task_fd_t *jlos_task_fd_get(jlos_task_t *task, int32_t fd);
void jlos_task_fd_free(jlos_task_t *task, int32_t fd);

void jlos_task_set_ready(jlos_task_t *t);
void jlos_task_set_running(jlos_task_t *t);
void jlos_task_set_blocked(jlos_task_t *t);
void jlos_task_set_zombie(jlos_task_t *t, uint32_t exit_code);
void jlos_task_set_waiting(jlos_task_t *t, uint32_t pid);

#define JLOS_TASK_SET_READY     jlos_task_set_ready
#define JLOS_TASK_SET_RUNNING   jlos_task_set_running
#define JLOS_TASK_SET_BLOCKED   jlos_task_set_blocked
#define JLOS_TASK_SET_ZOMBIE    jlos_task_set_zombie
#define JLOS_TASK_SET_WAITING   jlos_task_set_waiting

void jlos_task_sleep_until(jlos_task_manager_t *self, uint32_t wake_tick);
const char *jlos_task_status_map_str(uint32_t status);

void jlos_task_manager_init(jlos_task_manager_t* self);
void jlos_task_manager_destroy(jlos_task_manager_t* self);
bool jlos_task_manager_add_task(jlos_task_manager_t* self, jlos_task_t *task);

void jlos_task_manager_schedule(jlos_task_manager_t* self);

jlos_task_t *jlos_task_manager_find_pid(jlos_task_manager_t *self, uint32_t pid);
jlos_task_t *jlos_task_manager_curr_task_on_tick(jlos_task_manager_t *self);

jlos_task_t *jlos_process_fork(jlos_task_manager_t *self, jlos_task_t *parent,
    uint32_t fork_esp_ref, uint32_t fork_resume_pc);
int jlos_process_exec(jlos_task_t *task, void (*entrypoint)(void));
void jlos_process_exit(jlos_task_t *task, uint32_t exit_code);

bool jlos_need_resched(void);
void jlos_sched_set_need_resched(void);
void jlos_sched_wake_waiter(jlos_task_manager_t *self, uint32_t exited_pid);
#endif
