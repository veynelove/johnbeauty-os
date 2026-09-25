#include <lib/syscall.h>

int32_t syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3)
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
