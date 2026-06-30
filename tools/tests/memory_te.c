#include <tools/tests/memory_te.h>
#include <kernel/memory_manager.h>
#include <common/multiboot.h>

extern void printf(const char *);
extern void printf_hex(uint8_t);
extern void printf_hex32(uint32_t);

void memory_manager_test(const void *multiboot_structure)
{
    uint32_t *memupper = (uint32_t *)((size_t)multiboot_structure + 8);
    printf("multiboot_structure: 0x");
    printf_hex32((uint32_t)multiboot_structure);
    printf(" ");

    size_t test_heap_size = 64 * 1024;
    uint8_t *main_heap_start = (uint8_t*)(1024 * ((size_t)(*memupper) - 1024 * 16));
    uint8_t *heap = main_heap_start - test_heap_size - 4096;
    size_t m_size = test_heap_size;
    printf("heap: 0x");
    printf_hex(((size_t)heap >> 24) & 0xFF);
    printf_hex(((size_t)heap >> 16) & 0xFF);
    printf_hex(((size_t)heap >> 8) & 0xFF);
    printf_hex((size_t)heap & 0xFF);
    printf(" ");
    printf("size: 0x");
    printf_hex32(m_size);
    printf("\n");

    jlos_memory_manager_t *old_manager = jlos_active_memory_manager;
    jlos_memory_manager_t memory_manager_;
    jlos_memory_manager_init(&memory_manager_, heap, m_size);
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
    jlos_free(m_allocated);
    jlos_memory_manager_destroy(&memory_manager_);
    jlos_active_memory_manager = old_manager;
}