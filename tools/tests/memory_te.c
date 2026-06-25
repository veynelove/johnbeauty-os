#include <tools/tests/memory_te.h>
#include <kernel/memory_manager.h>

extern void printf_hex(uint8_t);

void memory_manager_test(const void *multiboot_structure)
{
    uint32_t *memupper = (uint32_t *)((size_t)multiboot_structure + 8);
    uint8_t *heap = (uint8_t*)JLOS_HEAP_START;
    jlos_memory_manager_t memory_manager_;
    jlos_memory_manager_init(&memory_manager_, heap, (*memupper) * 1024 - (size_t)heap - 10*1024);
    printf("heap: 0x");
    printf_hex(((size_t)heap >> 24) & 0xFF);
    printf_hex(((size_t)heap >> 16) & 0xFF);
    printf_hex(((size_t)heap >> 8) & 0xFF);
    printf_hex((size_t)heap & 0xFF);
    void *m_allocated = jlos_malloc(1024);
    printf("\nallocated: 0x");
    printf_hex(((size_t)m_allocated >> 24) & 0xFF);
    printf_hex(((size_t)m_allocated >> 16) & 0xFF);
    printf_hex(((size_t)m_allocated >> 8) & 0xFF);
    printf_hex((size_t)m_allocated & 0xFF);
    printf("\n");
}