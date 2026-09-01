#ifndef __COMMON_TYPES_H
#define __COMMON_TYPES_H

#include <tools/config.h>

typedef unsigned char              uint8_t;
typedef unsigned short             uint16_t;
typedef unsigned int               uint32_t;
typedef unsigned long long int     uint64_t;

typedef char                       int8_t;
typedef short                      int16_t;
typedef int                        int32_t;
typedef long long int              int64_t;

typedef unsigned char               bool;
#define true                        1
#define false                       0
#define NULL                        ((void*)0)

typedef const char *               string;
typedef uint32_t                   size_t;

#define JLOS_HEAP_START ((uint8_t)(10 * 1024 * 1024))
#define JLOS_HEAP_RESERVED_SIZE ((size_t)(10 * 1024))
#define JLOS_NET_MAX_SLOTS          65535
#define JLOS_NET_HASH_CHAIN_NUM 64

#define BYTES_TO_BE32(x4, x3, x2, x1) \
    ((((uint32_t)(x4) & 0xFF) << 24) | \
     (((uint32_t)(x3) & 0xFF) << 16) | \
     (((uint32_t)(x2) & 0xFF) << 8)  | \
      ((uint32_t)(x1) & 0xFF))

#define JLOS_EXCEPT_CEIL(a, b) (((a) + (b) - 1) / (b))
#define JLOS_ARRAY_LIMIT_RANGE(idx, range) (((idx) + 1) % (range))
#define JLOS_ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))
#define JLOS_ALIGN_DOWN(addr, align) ((uint32_t)(addr) & ~((align) - 1))

#endif