#ifndef __JLOS_KERNEL_SYSCALL_H
#define __JLOS_KERNEL_SYSCALL_H

#include <common/types.h>

#define JLOS_USER_SPACE_START   0x00000000
#define JLOS_USER_SPACE_END     0xBFFEFFFF


bool jlos_copy_from_user(void *dst, const void *usr_src, size_t n);
bool jlos_copy_to_user(void *usr_dst, const void *ker_src, size_t n);
bool jlos_access_ok(const void *addr, size_t n);
#endif
