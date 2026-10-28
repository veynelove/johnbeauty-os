/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dsa/sort.h>

static void jlos_sort_swap(uint8_t *a, uint8_t *b, size_t size)
{
    size_t n = size;
    while (n--) {
        uint8_t t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static void jlos_heap_sift_down(uint8_t *base, size_t root, size_t count, size_t size, jlos_sort_cmp_t cmp)
{
    while (root * 2 + 1 < count) {
        size_t child = root * 2 + 1;
        if (child + 1 < count && cmp(base + child * size, base + (child + 1) * size) < 0) {
            child++;
        }
        if (cmp(base + root * size, base + child * size) < 0) {
            jlos_sort_swap(base + root * size, base + child * size, size);
            root = child;
        } else {
            return;
        }
    }
}

void jlos_heap_sort(void *base, size_t count, size_t size, jlos_sort_cmp_t cmp)
{
    uint8_t *bytes = (uint8_t *)base;
    if (count < 2 || size == 0) {
        return;
    }
    for (size_t start = count / 2; start > 0; start--) {
        jlos_heap_sift_down(bytes, start - 1, count, size, cmp);
    }
    for (size_t end = count - 1; end > 0; end--) {
        jlos_sort_swap(bytes, bytes + end * size, size);
        jlos_heap_sift_down(bytes, 0, end, size, cmp);
    }
}

void jlos_insertion_sort(void *base, size_t count, size_t size, jlos_sort_cmp_t cmp)
{
    uint8_t *bytes = (uint8_t *)base;
    for (size_t i = 1; i < count; i++) {
        for (size_t j = i; j > 0 && cmp(bytes + (j - 1) * size, bytes + j * size) > 0; j--) {
            jlos_sort_swap(bytes + (j - 1) * size, bytes + j * size, size);
        }
    }
}
