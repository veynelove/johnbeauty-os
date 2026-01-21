#include <kernel/memory_manager.h>

namespace JLOS {
namespace Kernel {
memory_manager *memory_manager::active_memory_manager = nullptr;
memory_manager::memory_manager(uint8_t *start, size_t m_size)
{
     active_memory_manager = this;
     if (m_size < sizeof(memory_chunk)) {
          first = nullptr;
     } else {
          first = (memory_chunk *)start;
          first->m_allocated = false;
          first->prev = nullptr;
          first->next = nullptr;
          first->m_size = m_size - sizeof(memory_chunk);
     }
}

memory_manager::~memory_manager()
{
     if (active_memory_manager == this) {
          active_memory_manager = nullptr;
     }
}

void *memory_manager::malloc(size_t m_size)
{
     memory_chunk *result = nullptr;
     for (memory_chunk *chunk = first; chunk != nullptr && result == nullptr; chunk = chunk->next) {
          if (chunk->m_size > m_size && !chunk->m_allocated) {
               result = chunk;
          }
     }
     if (!result) {
          return nullptr;
     }
     if (result->m_size >= m_size + sizeof(memory_chunk) + 1) {
          memory_chunk *temp = (memory_chunk *)((size_t)result + sizeof(memory_chunk) + m_size);
          temp->m_allocated = false;
          temp->m_size = result->m_size - m_size - sizeof(memory_chunk);
          temp->prev = result;
          temp->next = result->next;
          if (temp->next) {
               temp->next->prev = temp;
          }
          result->m_size = m_size;
          result->next = temp;
     }
     result->m_allocated = true;
     return (void *)(((size_t)result) + sizeof(memory_chunk));
}

void memory_manager::free(void *ptr)
{
     memory_chunk *chunk = (memory_chunk *)((size_t)ptr - sizeof(memory_chunk));
     chunk->m_allocated = false;
     if (chunk->prev && chunk->prev->m_allocated) {
          chunk->prev->next = chunk->next;
          chunk->prev->m_size += chunk->m_size + sizeof(memory_chunk);
          if (chunk->next) {
               chunk->next->prev = chunk->prev;
          }
          chunk = chunk->prev;
     }
     if (chunk->next && chunk->next->m_allocated) {
          chunk->m_size += chunk->next->m_size + sizeof(memory_chunk);
          chunk->next = chunk->next->next;
          if (chunk->next) { 
               chunk->next->prev = chunk;
          }
     }
}
}
}

void *operator new(unsigned m_size)
{
     if (!JLOS::Kernel::memory_manager::active_memory_manager) {
          return nullptr;
     }
     return JLOS::Kernel::memory_manager::active_memory_manager->malloc(m_size);
}

void *operator new[](unsigned m_size)
{
     if (!JLOS::Kernel::memory_manager::active_memory_manager) {
          return nullptr;
     }
     return JLOS::Kernel::memory_manager::active_memory_manager->malloc(m_size);
}

void *operator new(unsigned m_size, void *ptr)
{
     return ptr;
}

void *operator new[](unsigned m_size, void *ptr)
{
     return ptr;
}

void operator delete(void *ptr)
{
     if (JLOS::Kernel::memory_manager::active_memory_manager) {
          JLOS::Kernel::memory_manager::active_memory_manager->free(ptr);
     }
}

void operator delete[](void *ptr)
{
     if (JLOS::Kernel::memory_manager::active_memory_manager) {
          JLOS::Kernel::memory_manager::active_memory_manager->free(ptr);
     }
}
