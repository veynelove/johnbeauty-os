#ifndef __JLOS_KERNEL_MEMORY_MANAGER_H
#define __JLOS_KERNEL_MEMORY_MANAGER_H

#include <common/types.h>

namespace JLOS {
namespace Kernel {
struct memory_chunk {
     memory_chunk *next;
     memory_chunk *prev;
     bool m_allocated;
     size_t m_size;
};

class memory_manager {
private:
     memory_chunk *first;
public:
     static memory_manager *active_memory_manager;
     
     memory_manager(uint8_t *start, size_t m_size);
     ~memory_manager();

     void *malloc(size_t m_size);
     void free(void *ptr);
};
}
}

void *operator new(unsigned m_size);
void *operator new[](unsigned m_size);

// placement new
void *operator new(unsigned m_size, void *ptr);
void *operator new[](unsigned m_size, void *ptr);

void operator delete(void *ptr);
void operator delete[](void *ptr);
#endif
