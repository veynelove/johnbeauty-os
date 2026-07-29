#include <hal/user_syscall.h>

/* x86 用户态 syscall 入口：int $0x80
 * 寄存器约定：eax=调用号, ebx=arg1, ecx=arg2, edx=arg3, 返回值在 eax */
uint32_t jlos_user_syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    uint32_t result;
    __asm__ __volatile__(
        "int $0x80\n\t"
        : "=a"(result)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return result;
}
