#ifndef __COMMON_TYPES_H
#define __COMMON_TYPES_H

namespace JLOS {
typedef unsigned char              uint8_t;
typedef unsigned short             uint16_t;
typedef unsigned int               uint32_t;
typedef unsigned long long int     uint64_t;

typedef char                       int8_t;
typedef short                      int16_t;
typedef int                        int32_t;
typedef long long int              int64_t;

typedef const char *               string;
typedef uint32_t                   size_t;
}

namespace JLOS {
namespace Kernel {
constexpr uint8_t HEAP_STACT = (10 * 1024 * 1024);
constexpr size_t HEAP_RESERVED_SIZE = (10 * 1024);

#define BYTES_TO_BE32(x4, x3, x2, x1) \
    ((((uint32_t)(x4) & 0xFF) << 24) | \
     (((uint32_t)(x3) & 0xFF) << 16) | \
     (((uint32_t)(x2) & 0xFF) << 8)  | \
      ((uint32_t)(x1) & 0xFF))
}
}
#endif
