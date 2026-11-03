/**
 * Copyright 2026 veyne.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JLCY_TYPE_H
#define _JLCY_TYPE_H

typedef unsigned char              uint8_t;
typedef unsigned short             uint16_t;
typedef unsigned int               uint32_t;
typedef unsigned long long int     uint64_t;

typedef char                       int8_t;
typedef short                      int16_t;
typedef int                        int32_t;
typedef long long int              int64_t;

#if __SIZEOF_POINTER__ == 8
typedef uint64_t                   uintptr_t;
typedef int64_t                    intptr_t;
#else
typedef uint32_t                   uintptr_t;
typedef int32_t                    intptr_t;
#endif

typedef uint32_t                   size_t;

#endif
