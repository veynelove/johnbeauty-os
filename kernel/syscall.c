#include <kernel/syscall.h>

bool jlos_access_ok(const void *addr, size_t n)
{
    uint32_t start = (uint32_t)addr;
    uint32_t end = start + n;
    if (start < JLOS_USER_SPACE_START || end > JLOS_USER_SPACE_END) {
        return false;
    }
    return true;
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
