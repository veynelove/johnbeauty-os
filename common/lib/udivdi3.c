#include <common/types.h>

uint64_t __udivdi3(uint64_t dividend, uint64_t divisor) 
    __attribute__((weak, alias("__udivdi3_generic")));
uint64_t __udivmoddi4(uint64_t dividend, uint64_t divisor, uint64_t *remainder)
    __attribute__((weak, alias("__udivmoddi4_generic")));

uint64_t __udivdi3_generic(uint64_t dividend, uint64_t divisor)
{
    if (divisor == 0) {
        return 0;
    }
    uint64_t quotient = 0;
    uint64_t bit = 1;
    while (divisor < dividend && !(divisor & 0x8000000000000000ULL)) {
        divisor <<= 1;
        bit <<= 1;
    }
    while (bit != 0) {
        if (dividend >= divisor) {
            dividend -= divisor;
            quotient |= bit;
        }
        divisor >>= 1;
        bit >>= 1;
    }
    return quotient;
}

uint64_t __udivmoddi4_generic(uint64_t dividend, uint64_t divisor, uint64_t *remainder)
{
    if (divisor == 0) {
        if (remainder) {
            *remainder = 0;
        }
        return 0;
    }
    uint64_t quotient = 0;
    uint64_t bit = 1;
    while (divisor < dividend && !(divisor & 0x8000000000000000ULL)) {
        divisor <<= 1;
        bit <<= 1;
    }
    while (bit != 0) {
        if (dividend >= divisor) {
            dividend -= divisor;
            quotient |= bit;
        }
        divisor >>= 1;
        bit >>= 1;
    }
    if (remainder) {
        *remainder = dividend;
    }
    return quotient;
}
