#include <common/types.h>

void jlos_memset(void *ptr, uint8_t value, size_t size) {
    uint8_t *p = (uint8_t*)ptr;
    while (size > 0 && ((size_t)p & 3)) {
        *p++ = value;
        size--;
    }
    uint32_t val32 = value | (value << 8) | (value << 16) | (value << 24);
    uint32_t *p32 = (uint32_t *)p;
    while (size >= 4) {
        *p32++ = val32;
        size -= 4;
    }
    p = (uint8_t *)p32;
    while (size > 0) {
        *p++ = value;
        size--;
    }
}

void *jlos_memcpy(void *dst, const void *src, size_t size)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (size > 0 && ((size_t)d & 3)) {
        *d++ = *s++;
        size--;
    }
    uint32_t *d32 = (uint32_t *)d;
    const uint32_t *s32 = (const uint32_t *)s;
    while (size >= 4) {
        *d32++ = *s32++;
        size -= 4;
    }
    d = (uint8_t *)d32;
    s = (const uint8_t *)s32;
    while (size > 0) {
        *d++ = *s++;
        size--;
    }
    return dst;
}

size_t jlos_strlcpy(char *dst, const char *src, size_t dsize)
{
    const char *osrc = src;
    size_t nleft = dsize;
    if (nleft) while (--nleft) if (!(*dst++ = *src++)) break;
    if (!nleft) {if (dsize) *dst = 0; while (*src++);}
    return (size_t)(src - osrc - 1);
}

void *memcpy(void *dst, const void *src, size_t size) __attribute__((weak, alias("jlos_memcpy")));
