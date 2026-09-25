#include <kernel/syscall.h>
#include <kernel/paging.h>
#include <kernel/memory_manager.h>
#include <kernel/ipc.h>
#include <kernel/page_frame_allocator.h>
#include <kernel/initcall.h>
#include <kernel/multitask.h>
#include <hal/timer.h>
#include <hal/kernel_syscall.h>
#include <hal/paging.h>
#include <filesystem/vfs.h>

#define JLOS_KERNEL_LOG_SUBSYS "syscall"
#include <kernel/printk.h>

extern jlos_task_manager_t      *g_task_manager_ptr;
extern jlos_task_t              *g_current_task_ptr;

static jlos_syscall_handler_t   s_syscall_handler;
jlos_syscall_handler_t          *s_syscall_handler_ptr = &s_syscall_handler;

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
        buf = (uint8_t *)jlos_kalloc(len);
        if (!buf) {
            return -SYSCALL_ENOMEM;
        }
    }
    if (!jlos_copy_from_user(buf, user_buf, len)) {
        if (buf != stack_buf) {
            jlos_kfree(buf);
        }
        return -SYSCALL_EFAULT;
    }

    switch (fd_entry->type) {
        case JLOS_TASK_FD_CONSOLE : {
            buf[len - 1] = '\0';
            printf((const char *)buf);
            if (buf != stack_buf) {
                jlos_kfree(buf);
            }
            return len;
        }
        case JLOS_TASK_FD_PIPE : {
            jlos_pipe_t *pipe = (jlos_pipe_t *)fd_entry->obj;
            uint32_t written = jlos_pipe_write(pipe, buf, len);
            if (buf != stack_buf) {
                jlos_kfree(buf);
            }
            return written;
        }
        case JLOS_TASK_FD_FILE : {
            jlos_vfs_file_t *file = (jlos_vfs_file_t *)fd_entry->obj;
            int32_t written = jlos_vfs_write(file, buf, len);
            if (buf != stack_buf) {
                jlos_kfree(buf);
            }
            if (written < 0) {
                return - SYSCALL_EIO;
            }
            return written;
        }
        default :
            break;
    }
    
    if (buf != stack_buf) {
        jlos_kfree(buf);
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
            buf = (uint8_t *)jlos_kalloc(len);
            if (!buf) {
                return -SYSCALL_ENOMEM;
            }
        }
        uint32_t read = jlos_pipe_read(pipe, buf, len);
        if (!jlos_copy_to_user(user_buf, buf, read)) {
            if (buf != stack_buf) {
                jlos_kfree(buf);
            }
            return -SYSCALL_EFAULT;
        }
        if (buf != stack_buf) {
            jlos_kfree(buf);
        }
        return read;
    }
    if (fd_entry->type == JLOS_TASK_FD_FILE) {
        jlos_vfs_file_t *file = (jlos_vfs_file_t *)fd_entry->obj;
        uint8_t stack_buf[256];
        uint8_t *buf = NULL;
        if (len <= sizeof(stack_buf)) {
            buf = stack_buf;
        } else {
            buf = (uint8_t *)jlos_kalloc(len);
            if (!buf) {
                return -SYSCALL_ENOMEM;
            }
        }
        int32_t read = jlos_vfs_read(file, buf, len);
        if (read < 0) {
            if (buf != stack_buf) {
                jlos_kfree(buf);
            }
            return -SYSCALL_EIO;
        }
        if (!jlos_copy_to_user(user_buf, buf, (uint32_t)read)) {
            if (buf != stack_buf) {
                jlos_kfree(buf);
            }
            return -SYSCALL_EFAULT;
        }
        if (buf != stack_buf) {
            jlos_kfree(buf);
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
    jlos_pipe_t *pipe = (jlos_pipe_t *)jlos_kalloc(sizeof(jlos_pipe_t));
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
    jlos_pipe_ref_inc(pipe);
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
            jlos_pipe_ref_dec(pipe);
        }
    }
    if (fd_entry->type == JLOS_TASK_FD_FILE) {
        jlos_vfs_file_t *file = (jlos_vfs_file_t *)fd_entry->obj;
        if (file) {
            jlos_vfs_close(file);
        }
    }
    jlos_task_fd_free(g_current_task_ptr, fd);
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_open(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr || !g_current_task_ptr->fds) {
        return -SYSCALL_ENOMEM;
    }
    char kernel_path[JLOS_VFS_PATH_MAX];
    for (uint32_t i = 0; i < JLOS_VFS_PATH_MAX; i++) {
        if (!jlos_copy_from_user(&kernel_path[i], (const void *)(arg1 + i), 1)) {
            return -SYSCALL_EFAULT;
        }
        if (kernel_path[i] == '\0') {
            break;
        }
    }
    kernel_path[JLOS_VFS_PATH_MAX - 1] = '\0';
    jlos_vfs_file_t *file = jlos_vfs_open(kernel_path, arg2, arg3);
    if (!file) {
        return -SYSCALL_ENOENT;
    }
    int32_t fd = jlos_task_fd_malloc(g_current_task_ptr);
    if (fd < 0) {
        jlos_vfs_close(file);
        return -SYSCALL_ENOMEM;
    }
    jlos_task_fd_t *fd_entry = jlos_task_fd_get(g_current_task_ptr, fd);
    fd_entry->type = JLOS_TASK_FD_FILE;
    fd_entry->obj = file;
    switch (arg2 & JLOS_VFS_O_ACCMODE) {
        case JLOS_VFS_O_WRONLY :
            fd_entry->flags = JLOS_TASK_FD_WRITE_ONLY;
            break;
        case JLOS_VFS_O_RDWR :
            fd_entry->flags = JLOS_TASK_FD_READ_WRITE;
            break;
        default:
            fd_entry->flags = JLOS_TASK_FD_READ_ONLY;
            break;
    }
    return fd;
}

static int32_t syscall_lseek(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    int32_t fd = (int32_t)arg1;
    if (!g_current_task_ptr || !g_current_task_ptr->fds) {
        return -SYSCALL_ENOMEM;
    }
    jlos_task_fd_t *fd_entry = jlos_task_fd_get(g_current_task_ptr, fd);
    if (!fd_entry || fd_entry->type != JLOS_TASK_FD_FILE) {
        return -SYSCALL_ENINVAL;
    }
    jlos_vfs_file_t *file = (jlos_vfs_file_t *)fd_entry->obj;
    int32_t result = jlos_vfs_lseek(file, (int32_t)arg2, (int32_t)arg3);
    if (result < 0) {
        return -SYSCALL_ENINVAL;
    }
    return result;
}

static int32_t syscall_fork(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;
    if (!g_current_task_ptr || !g_task_manager_ptr) {
        return -SYSCALL_ENOMEM;
    }
    jlos_cpu_state_t *tf = g_current_task_ptr->syscall_tf;
    if (!tf) {
        return -SYSCALL_ENOMEM;
    }
    jlos_task_t *child = jlos_process_fork(g_task_manager_ptr, g_current_task_ptr, tf);
    if (!child) {
        return -SYSCALL_ENOMEM;
    }
    return (int32_t)child->pid;
}

static int32_t syscall_clone(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t clone_flags = arg1;
    uint32_t child_stack = arg2;
    (void)arg3;
    if (!g_current_task_ptr || !g_task_manager_ptr) {
        return -SYSCALL_ENOMEM;
    }
    jlos_cpu_state_t *tf = g_current_task_ptr->syscall_tf;
    if (!tf) {
        return -SYSCALL_ENOMEM;
    }
    jlos_task_t *child =
        jlos_process_clone(g_task_manager_ptr, g_current_task_ptr, tf, clone_flags, child_stack);
    if (!child) {
        return -SYSCALL_ENOMEM;
    }
    return (int32_t)child->pid;
}

static int32_t syscall_unlink(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    char kernel_path[JLOS_VFS_PATH_MAX];
    for (uint32_t i = 0; i < JLOS_VFS_PATH_MAX; i++) {
        if (!jlos_copy_from_user(&kernel_path[i], (const void *)(arg1 + i), 1)) {
            return -SYSCALL_EFAULT;
        }
        if (kernel_path[i] == '\0') {
            break;
        }
    }
    kernel_path[JLOS_VFS_PATH_MAX - 1] = '\0';
    if (jlos_vfs_unlink(kernel_path) != 0) {
        return -SYSCALL_ENOENT;
    }
    (void)arg2;
    (void)arg3;
    return 0;
}

static int32_t syscall_task_brk(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr || !g_current_task_ptr->mm) {
        return -SYSCALL_ENOMEM;
    }
    jlos_mm_t *mm = g_current_task_ptr->mm;
    if (arg1 == 0) {
        return (int32_t)mm->brk_end;
    }
    uint32_t new_brk = arg1;
    if (new_brk < mm->brk_start || new_brk > mm->brk_limit) {
        return (int32_t)mm->brk_end;
    }
    uint32_t old_brk = mm->brk_end;
    
    if (new_brk > old_brk) {
        jlos_vma_grow_tail(mm,
            mm->brk_start, new_brk, JLOS_VMA_WRITE | JLOS_VMA_USER, JLOS_VMA_TYPE_BRK);
    } else if (new_brk < old_brk) {
        jlos_vma_shrink_tail(mm, new_brk, JLOS_VMA_TYPE_BRK);
    }
    mm->brk_end = new_brk;
    (void)arg2;
    (void)arg3;
    return (int32_t)new_brk;
}

static int32_t syscall_mmap(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    (void)arg2;
    (void)arg3;
    struct {
        uint32_t addr;
        uint32_t len;
        uint32_t prot;
        uint32_t flags;
        uint32_t fd;
        uint32_t offset;
    } args;
    if (!g_current_task_ptr || !g_current_task_ptr->mm) {
        return -SYSCALL_ENOMEM;
    }
    if (!jlos_copy_from_user(&args, (const void *)arg1, sizeof(args))) {
        return -SYSCALL_EFAULT;
    }
    if (args.len == 0 || args.prot == 0) {
        return -SYSCALL_ENINVAL;
    }
    if (!(args.flags & JLOS_MMAP_ANON)) {
        return -SYSCALL_ENOSYS;
    }
    uint32_t len = JLOS_PAGE_ALIGN_UP(args.len);
    jlos_mm_t *mm = g_current_task_ptr->mm;

    uint32_t addr;
    if (args.flags & JLOS_MMAP_FIXED) {
        if (args.addr & (JLOS_PAGE_FRAME_SIZE - 1)) {
            return -SYSCALL_ENINVAL;
        }
        if (args.addr + len < args.addr || !jlos_access_ok((const void *)args.addr, len)) {
            return -SYSCALL_ENINVAL;
        }
        addr = args.addr;
        jlos_vma_remove_range(mm, addr, addr + len);
    } else {
        uint32_t hint = args.addr ? args.addr : 0;
        addr = jlos_vma_find_free_area(mm, JLOS_TASK_USER_MMAP_BASE, JLOS_TASK_USER_MMAP_LIMIT, hint, len);
        if (!addr) {
            return -SYSCALL_ENOMEM;
        }
    }
    uint32_t vma_flags = JLOS_VMA_USER;
    if (args.prot & JLOS_PROT_READ)     vma_flags |= JLOS_VMA_READ;
    if (args.prot & JLOS_PROT_WRITE)    vma_flags |= JLOS_VMA_WRITE;
    if (args.prot & JLOS_PROT_EXEC)     vma_flags |= JLOS_VMA_EXEC;
    if (!jlos_vma_add(mm, addr, addr + len, vma_flags, JLOS_VMA_TYPE_ANON)) {
        return -SYSCALL_ENOMEM;
    }
    return (int32_t)addr;
}

static int32_t syscall_munmap(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    (void)arg3;
    if (!g_current_task_ptr || !g_current_task_ptr->mm) {
        return -SYSCALL_ENOMEM;
    }
    uint32_t addr = arg1;
    uint32_t len = arg2;
    if (len == 0 || (addr & (JLOS_PAGE_FRAME_SIZE - 1))) {
        return -SYSCALL_ENINVAL;
    }
    jlos_vma_remove_range(g_current_task_ptr->mm, addr, addr + len);
    return 0;
}

static int syscall_copy_strings(uint32_t user_ptr_arr, char *kernel_ptrs[], char *strbuf, size_t *strbuf_off, int *count)
{
    if (!user_ptr_arr) {
        return 0;
    }
    *count = 0;
    for (int i = 0; i < JLOS_EXECVE_MAX_ARGS; i++) {
        kernel_ptrs[i] = NULL;
    }
    for (int i = 0; i < JLOS_EXECVE_MAX_ARGS; i++) {
        uint32_t user_str;
        if (!jlos_copy_from_user(&user_str, (const void *)(user_ptr_arr + i * sizeof(uint32_t)), sizeof(uint32_t))) {
            return - SYSCALL_EFAULT;
        }
        if (user_str == 0) {
            break;
        }
        if (*strbuf_off + JLOS_EXECVE_MAX_STRLEN > (size_t)JLOS_EXECVE_MAX_ARGS * JLOS_EXECVE_MAX_STRLEN) {
            return - SYSCALL_EPBIG;
        }
        char *kstr = strbuf + *strbuf_off;
        for (uint32_t j = 0; j < JLOS_EXECVE_MAX_STRLEN; j++) {
            if (!jlos_copy_from_user(&kstr[j], (const void *)(user_str + j), 1)) {
                return -SYSCALL_EFAULT;
            }
            if (kstr[j] == '\0') {
                break;
            }
        }
        kstr[JLOS_EXECVE_MAX_STRLEN - 1] = '\0';
        *strbuf_off += jlos_strlen(kstr) + 1;
        kernel_ptrs[i] = kstr;
        *count = i + 1;
    }
    return 0;
}

static int32_t syscall_execve(uint32_t path, uint32_t argv, uint32_t envp)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENINVAL;
    }
    char kernel_path[JLOS_VFS_PATH_MAX];
    for (uint32_t i = 0; i < JLOS_VFS_PATH_MAX; i++) {
        if (!jlos_copy_from_user(&kernel_path[i], (const void *)(path + i), 1)) {
            return -SYSCALL_EFAULT;
        }
        if (kernel_path[i] == '\0') {
            break;
        }
    }
    kernel_path[JLOS_VFS_PATH_MAX - 1] = '\0';
    char *strbuf = (char *)jlos_kalloc(((size_t)JLOS_EXECVE_MAX_ARGS * JLOS_EXECVE_MAX_STRLEN));
    if (!strbuf) {
        return -SYSCALL_ENOMEM;
    }
    size_t strbuf_off = 0;
    char *kernel_argv[JLOS_EXECVE_MAX_ARGS];
    char *kernel_envp[JLOS_EXECVE_MAX_ARGS];
    int argc = 0;
    int envc = 0;
    int err = syscall_copy_strings(argv, kernel_argv, strbuf, &strbuf_off, &argc);
    if (err == 0) {
        err = syscall_copy_strings(envp, kernel_envp, strbuf, &strbuf_off, &envc);
    }
    if (err < 0) {
        jlos_kfree(strbuf);
        return err;
    }
    int ret = jlos_process_exec_elf(g_current_task_ptr, kernel_path, argc, kernel_argv, kernel_envp);
    jlos_kfree(strbuf);
    return ret;
}

static int32_t syscall_exit(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (!g_current_task_ptr) {
        return -SYSCALL_ENOMEM;
    }
    jlos_process_exit(g_current_task_ptr, arg1);
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
    jlos_task_sleep_until(g_task_manager_ptr, jlos_hal_timer_get_ticks() + arg1);
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
    printk_info("--- task list ---\n");
    for (uint32_t i = 0; i < g_task_manager_ptr->num_tasks; i++) {
        jlos_task_t *t = g_task_manager_ptr->tasks[i];
        if (t) {
            printk_info("[%d] name = %s, pid = %u, status = %s, task_type = %s\n", i, t->name,
                t->pid, jlos_task_status_map_str(t->status), t->is_user_process ? "user" : "kernel");
        }
    }
    printk_info("---\n");
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
    while (target->status != JLOS_TASK_ZOMBIE) {
        JLOS_TASK_SET_WAITING(g_current_task_ptr, target_pid);
        jlos_task_manager_schedule(g_task_manager_ptr);
        target = jlos_task_manager_find_pid(g_task_manager_ptr, target_pid);
        if (!target) {
            return -SYSCALL_ENINVAL;
        }
    }
    if (exit_code && !jlos_copy_to_user(exit_code, &target->exit_code, sizeof(int32_t))) {
            return -SYSCALL_EFAULT;
    }
    (void)arg3;
    uint32_t reaped_pid = target->pid;

    jlos_task_free(g_task_manager_ptr, target);

    return (int32_t)reaped_pid;
}

void jlos_syscall_register(uint8_t num, jlos_syscall_func_t handler)
{
    if (!s_syscall_handler_ptr) {
        return;
    }
    s_syscall_handler_ptr->dispatch[num] = handler;
}

static int32_t s_syscall_dispatch(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3)
{
    return jlos_syscall_do_dispatch(s_syscall_handler_ptr, num, a1, a2, a3);
}

static uint32_t s_syscall_resched_do(uint32_t ctx)
{
    if (g_current_task_ptr) {
        g_current_task_ptr->yield = false;
        jlos_task_manager_schedule(g_task_manager_ptr);
    }
    return ctx;
}

void jlos_syscall_handler_init(void)
{
    jlos_syscall_handler_t *self = &s_syscall_handler;
    self->interrupt_manager = jlos_active_irq_manager;
    self->interrupt_number = KERNEL_SYSCALL_INTERRUPT_NUM;
    jlos_irq_handler_init((jlos_irq_handler_t *)self, jlos_active_irq_manager, KERNEL_SYSCALL_INTERRUPT_NUM);
    
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
    jlos_syscall_register(JLOS_SYSCALL_MMAP, syscall_mmap);
    jlos_syscall_register(JLOS_SYSCALL_MUNMAP, syscall_munmap);
    jlos_syscall_register(JLOS_SYSCALL_EXECVE, syscall_execve);
    jlos_syscall_register(JLOS_SYSCALL_OPEN, syscall_open);
    jlos_syscall_register(JLOS_SYSCALL_LSEEK, syscall_lseek);
    jlos_syscall_register(JLOS_SYSCALL_UNLINK, syscall_unlink);
    jlos_syscall_register(JLOS_SYSCALL_FORK, syscall_fork);
    jlos_syscall_register(JLOS_SYSCALL_CLONE, syscall_clone);
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
    if (g_task_manager_ptr && g_task_manager_ptr->need_resched) {
        return true;
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
        g_current_task_ptr && g_current_task_ptr->mm ? g_current_task_ptr->mm->pc : jlos_hal_paging_get_active_context();
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

JLOS_INITCALL(JLOS_INITCALL_DEVICE, jlos_syscall_handler_init);
