/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_DSA_SORT_H
#define _JLOS_DSA_SORT_H

#include <common/types.h>

typedef int (*jlos_sort_cmp_t)(const void *a, const void *b);

void jlos_heap_sort(void *base, size_t count, size_t size, jlos_sort_cmp_t cmp);
void jlos_insertion_sort(void *base, size_t count, size_t size, jlos_sort_cmp_t cmp);

#endif
