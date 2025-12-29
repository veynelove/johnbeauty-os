#ifndef __JLOS_KERNEL_MEMORYMANAGERMENT_H
#define __JLOS_KERNEL_MEMORYMANAGERMENT_H

#include <common/types.h>

namespace JLOS {
namespace Kernel {
struct MemoryChunk {
     MemoryChunk *next;
     MemoryChunk *prev;
     bool allocated;
     size_t size;
};

class MemoryManager {
private:
     MemoryChunk *first;
public:
     static MemoryManager *activeMemoryManager;
     
     MemoryManager(size_t start, size_t size);
     ~MemoryManager();

     void *malloc(size_t size);
     void free(void *ptr);
};
}
}

void *operator new(unsigned size);
void *operator new[](unsigned size);

// placement new
void *operator new(unsigned size, void *ptr);
void *operator new[](unsigned size, void *ptr);

void operator delete(void *ptr);
void operator delete[](void *ptr);
#endif
