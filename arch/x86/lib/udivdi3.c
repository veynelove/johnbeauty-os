#include <common/types.h>

uint64_t __udivdi3(uint64_t dividend, uint64_t divisor)
{
    if (divisor == 0) {
        return 0;
    }
    if ((divisor >> 32) == 0) {
        uint32_t d = (uint32_t)divisor;
        uint32_t n_hi = (uint32_t)(dividend >> 32);
        uint32_t n_lo = (uint32_t)dividend;
        uint32_t q_hi = n_hi / d;
        uint32_t r = n_hi % d;
        uint32_t q_lo;
        __asm__ __volatile__(
            "divl %4"
            : "=a"(q_lo), "=d"(r)
            : "a"(n_lo), "d"(r), "r"(d)
        );
        return ((uint64_t)q_hi << 32) | q_lo;
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

uint64_t __udivmoddi4(uint64_t dividend, uint64_t divisor, uint64_t *remainder)
{
    if (divisor == 0) {
        if (remainder) {
            *remainder = 0;
        }
        return 0;
    }
    if ((divisor >> 32) == 0) {
        uint32_t d = (uint32_t)divisor;
        uint32_t n_hi = (uint32_t)(dividend >> 32);
        uint32_t n_lo = (uint32_t)dividend;
        uint32_t q_hi = n_hi / d;
        uint32_t r = n_hi % d;
        uint32_t q_lo;
        __asm__ __volatile__(
            "divl %4"
            : "=a"(q_lo), "=d"(r)
            : "a"(n_lo), "d"(r), "r"(d)
        );
        if (remainder) {
            *remainder = r;
        }
        return ((uint64_t)q_hi << 32) | q_lo;
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
