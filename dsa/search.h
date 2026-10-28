/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLOS_DSA_SEARCH_H
#define _JLOS_DSA_SEARCH_H

#include <common/types.h>

typedef int (*jlos_search_cmp_t)(const void *key, const void *elem);

void *jlos_bsearch(const void *key, const void *base, size_t count, size_t size, jlos_search_cmp_t cmp);
void *jlos_lower_bound(const void *key, const void *base, size_t count, size_t size, jlos_search_cmp_t cmp);
void *jlos_upper_bound(const void *key, const void *base, size_t count, size_t size, jlos_search_cmp_t cmp);

#endif
