/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dsa/search.h>

void *jlos_bsearch(const void *key, const void *base, size_t count, size_t size, jlos_search_cmp_t cmp)
{
    uint8_t *found = (uint8_t *)jlos_lower_bound(key, base, count, size, cmp);
    uint8_t *end = (uint8_t *)base + count * size;
    if (found < end && cmp(key, found) == 0) {
        return found;
    }
    return nullptr;
}

void *jlos_lower_bound(const void *key, const void *base, size_t count, size_t size, jlos_search_cmp_t cmp)
{
    const uint8_t *lo = (const uint8_t *)base;
    size_t n = count;
    while (n > 0) {
        size_t half = n / 2;
        const uint8_t *mid = lo + half * size;
        if (cmp(key, mid) > 0) {
            lo = mid + size;
            n -= half + 1;
        } else {
            n = half;
        }
    }
    return (void *)lo;
}

void *jlos_upper_bound(const void *key, const void *base, size_t count, size_t size, jlos_search_cmp_t cmp)
{
    const uint8_t *lo = (const uint8_t *)base;
    size_t n = count;
    while (n > 0) {
        size_t half = n / 2;
        const uint8_t *mid = lo + half * size;
        if (cmp(key, mid) >= 0) {
            lo = mid + size;
            n -= half + 1;
        } else {
            n = half;
        }
    }
    return (void *)lo;
}
