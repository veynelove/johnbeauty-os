#include <common/types.h>

__attribute__((weak))
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

__attribute__((weak))
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

size_t jlos_strlen(const char *s)
{
    const char *p = s;
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}

int jlos_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int jlos_strncmp(const char *a, const char *b, size_t n)
{
    while (n > 0 && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char *jlos_strchr(const char *s, char c)
{
    while (*s) {
        if (*s == c) {
            return (char *)s;
        }
        s++;
    }
    if (c == 0) {
        return (char *)s;
    }
    return NULL;
}

int jlos_memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    while (n > 0) {
        if (*pa != *pb) {
            return (int)*pa - (int)*pb;
        }
        pa++;
        pb++;
        n--;
    }
    return 0;
}

void *jlos_memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        return jlos_memcpy(dst, src, n);
    }
    if (d > s) {
        d += n;
        s += n;
        while (n > 0) {
            *--d = *--s;
            n--;
        }
    }
    return dst;
}

uint32_t jlos_popcount32(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (x * 0x01010101u) >> 24;
}

void *memcpy(void *dst, const void *src, size_t size) __attribute__((weak, alias("jlos_memcpy")));
