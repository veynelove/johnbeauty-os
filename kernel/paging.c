#include <kernel/paging.h>
#include <kernel/memory_manager.h>
#include <hal/paging.h>
#include <kernel/page_frame_allocator.h>

jlos_paging_context_t *jlos_active_paging_context = NULL;
jlos_paging_context_t s_kernel_paging_context;

void jlos_paging_context_init(jlos_paging_context_t *self)
{
    self->page_dir = NULL;
    self->num_page_tables = 0;
}

void jlos_paging_context_destroy(jlos_paging_context_t *self)
{
    if (self->page_dir) {
        for (uint32_t i = 0; i < JLOS_PAGE_DIR_ENTRIES; i++) {
            jlos_page_dir_entry_t *pde = &self->page_dir->entries[i];
            if (*pde & JLOS_PDE_PRESENT) {
                jlos_page_frame_free((void *)((*pde) & ~0xFFF));
            }
        }
        jlos_page_frame_free(self->page_dir);
    }
    jlos_paging_context_init(self);
}

bool jlos_paging_map(jlos_paging_context_t *self, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    if (!self->page_dir) {
        self->page_dir = jlos_page_frame_malloc();
        if (!self->page_dir) {
            return false;
        }
        jlos_memset(self->page_dir, 0, sizeof(jlos_page_dir_t));
    }
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);

    jlos_page_table_t *page_table = NULL;
    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];

    if (*pde & JLOS_PDE_PRESENT) {
        page_table = (jlos_page_table_t *)((*pde) & ~0xFFF);
    } else {
        page_table = jlos_page_frame_malloc();
        if (!page_table) {
            return false;
        }
        jlos_memset(page_table, 0, sizeof(jlos_page_table_t));
        *pde = (uint32_t)page_table | JLOS_PDE_PRESENT | JLOS_PDE_WRITABLE | (flags & JLOS_PDE_USER);
    }
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    *pte = (physical_addr & ~0xFFF) | flags;

    jlos_hal_paging_flush_tlb(virtual_addr);
    return true;
}

bool jlos_paging_unmap(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self->page_dir) {
        return false;
    }
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);

    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return false;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)((*pde) & ~0xFFF);
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    *pte = 0;

    jlos_hal_paging_flush_tlb(virtual_addr);
    return true;
}

uint32_t jlos_paging_get_physical_addr(jlos_paging_context_t *self, uint32_t virtual_addr)
{
    if (!self->page_dir) {
        return virtual_addr;
    }
    uint32_t pd_index = jlos_paging_get_page_dir_index(virtual_addr);
    uint32_t pt_index = jlos_paging_get_page_table_index(virtual_addr);
    jlos_page_dir_entry_t *pde = &self->page_dir->entries[pd_index];
    if (!(*pde & JLOS_PDE_PRESENT)) {
        return 0;
    }
    jlos_page_table_t *page_table = (jlos_page_table_t *)((*pde) & ~0xFFF);
    jlos_page_table_entry_t *pte = &page_table->entries[pt_index];
    if (!(*pte & JLOS_PTE_PRESENT)) {
        return 0;
    }
    return ((*pte) & ~0xFFF) | jlos_paging_get_page_offset(virtual_addr);
}

void jlos_paging_enable(jlos_paging_context_t *self)
{
    jlos_active_paging_context = self;
    jlos_hal_paging_enable((uint32_t)self->page_dir);
}

void jlos_paging_disable(void)
{
    jlos_hal_paging_disable();
}

void jlos_paging_switch(jlos_paging_context_t *self)
{
    jlos_active_paging_context = self;
   jlos_hal_paging_switch((uint32_t)self->page_dir);
}

void jlos_paging_initialize_kernel_paging(void)
{
    jlos_paging_context_init(&s_kernel_paging_context);
    for (uint32_t addr = 0; addr < 0x10000000; addr += JLOS_PAGE_SIZE) {
        jlos_paging_map(&s_kernel_paging_context, addr, addr, JLOS_PTE_PRESENT | JLOS_PTE_WRITABLE);
    }
    jlos_paging_enable(&s_kernel_paging_context);
}
