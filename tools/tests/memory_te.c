#include <tools/tests/memory_te.h>
#include <kernel/memory_manager.h>
#include <common/multiboot.h>

extern void printf(const char *);
extern void printf_hex(uint8_t);
extern void printf_hex32(uint32_t);

void memory_manager_test(const void *multiboot_structure)
{
    printf("MEMORY test start\n");
#if KERNEL_CONFIG_DEBUG_MEMORY
    printf("multiboot_structure: 0x");
    printf_hex32((uint32_t)multiboot_structure);
    printf(" ");
#endif
    size_t test_heap_size = 64 * 1024;
    jlos_memory_manager_t *old_manager = jlos_active_memory_manager;
    uint8_t *heap = (uint8_t *)jlos_malloc(test_heap_size);
    if (!heap) {
        printf("MEMORY test: failed to allocate test heap\n");
        return;
    }
    size_t m_size = test_heap_size;
    
#if KERNEL_CONFIG_DEBUG_MEMORY
    printf("heap: 0x");
    printf_hex(((size_t)heap >> 24) & 0xFF);
    printf_hex(((size_t)heap >> 16) & 0xFF);
    printf_hex(((size_t)heap >> 8) & 0xFF);
    printf_hex((size_t)heap & 0xFF);
    printf(" ");
    printf("size: 0x");
    printf_hex32(m_size);
    printf("\n");
#endif

    jlos_memory_manager_t memory_manager_;
    jlos_memory_manager_init(&memory_manager_, heap, m_size);

#if KERNEL_CONFIG_DEBUG_MEMORY
    printf("heap: 0x");
    printf_hex(((size_t)heap >> 24) & 0xFF);
    printf_hex(((size_t)heap >> 16) & 0xFF);
    printf_hex(((size_t)heap >> 8) & 0xFF);
    printf_hex((size_t)heap & 0xFF);
#endif

    void *m_allocated = jlos_malloc(1024);

#if KERNEL_CONFIG_DEBUG_MEMORY
    printf("\nallocated: 0x");
    printf_hex(((size_t)m_allocated >> 24) & 0xFF);
    printf_hex(((size_t)m_allocated >> 16) & 0xFF);
    printf_hex(((size_t)m_allocated >> 8) & 0xFF);
    printf_hex((size_t)m_allocated & 0xFF);
    printf("\n");
#endif

    jlos_free(m_allocated);
    jlos_memory_manager_destroy(&memory_manager_);
    jlos_active_memory_manager = old_manager;
    jlos_free(heap);
}