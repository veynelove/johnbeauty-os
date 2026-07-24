#include <hal/paging.h>

void jlos_hal_paging_enable(uint32_t page_dir_physical_addr)
{
    __asm__ __volatile__(
        "movl %0, %%cr3\n\t"
        "movl %%cr0, %%eax\n\t"
        "orl $0x80000001, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        : : "r"(page_dir_physical_addr) : "eax"
    );
}

void jlos_hal_paging_disable(void)
{
    __asm__ __volatile__(
        "movl %%cr0, %%eax\n\t"
        "andl $~0x80000000, %%eax\n\t"
        "movl %%eax, %%cr0\n\t"
        : : : "eax"
    );
}

void jlos_hal_paging_switch(uint32_t page_dir_physical_addr)
{
    __asm__ __volatile__(
        "movl %0, %%cr3\n\t"
        : : "r"(page_dir_physical_addr) : "memory"
    );
}

void jlos_hal_paging_flush_tlb(uint32_t virtual_addr)
{
    __asm__ __volatile__(
        "invlpg (%0)\n\t"
        : : "r"(virtual_addr) : "memory"
    );
}

uint32_t jlos_hal_paging_get_fault_addr(void)
{
    uint32_t cr2;
    __asm__ __volatile__(
        "movl %%cr2, %0\n\t"
        : "=r"(cr2)
    );
    return cr2;
}
