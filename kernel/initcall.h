#ifndef _JLOS_KERNEL_INITCALL_H
#define _JLOS_KERNEL_INITCALL_H

typedef void (*jlos_initcall_fn_t)(void);

#define JLOS_INITCALL_CORE      0
#define JLOS_INITCALL_SUBSYS    1
#define JLOS_INITCALL_DEVICE    2
#define JLOS_INITCALL_LATE      3
#define JLOS_INITCALL_POST      4

#define __JLOS_INITCALL(level, fn) \
    static jlos_initcall_fn_t __initcall_##fn \
    __attribute__((used, section(".initcall" #level))) = fn

#define JLOS_INITCALL(level, fn) __JLOS_INITCALL(level, fn)

void jlos_do_initcalls(void);

#endif
