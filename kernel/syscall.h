#ifndef _JLOS_KERNEL_SYSCALL_H
#define _JLOS_KERNEL_SYSCALL_H

#include <common/types.h>
#include <hal/irq.h>
#include <hal/syscall_abi.h>

#define JLOS_USER_SPACE_START           0x00000000
#define JLOS_USER_SPACE_END             0xBFFEFFFF

#define JLOS_SYSCALL_WRITE_BUF_SIZE_MAX 4096

#define KERNEL_SYSCALL_INTERRUPT_NUM    0x80

typedef struct jlos_syscall_handler jlos_syscall_handler_t;

typedef int32_t (*jlos_syscall_func_t)(uint32_t arg1, uint32_t arg2, uint32_t arg3);
typedef uint32_t (*jlos_syscall_handle_interrupt_func_t)(jlos_syscall_handler_t*, uint32_t);

struct jlos_syscall_handler {
    uint8_t interrupt_number;
    jlos_irq_manager_t *interrupt_manager;
    uint32_t (*handle_interrupt)(jlos_syscall_handler_t*, uint32_t);
    jlos_syscall_func_t dispatch[JLOS_SYSCALL_MAX];
};

void jlos_syscall_handler_init(void);
void jlos_syscall_handler_destroy(jlos_syscall_handler_t* self);
int32_t jlos_syscall_do_dispatch(jlos_syscall_handler_t *self, uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3);
bool jlos_syscall_need_resched(void);

bool jlos_copy_from_user(void *dst, const void *usr_src, size_t n);
bool jlos_copy_to_user(void *usr_dst, const void *ker_src, size_t n);
bool jlos_access_ok(const void *addr, size_t n);
#endif
