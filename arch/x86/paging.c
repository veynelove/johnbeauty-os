#include <hal/paging.h>
#include <hal/smp.h>

static struct jlos_paging_context *s_active_paging_context[JLOS_MAX_CPUS];

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

void jlos_hal_paging_flush_all_tlb(void)
{
     uint32_t cr4;
    __asm__ __volatile__(
        "movl %%cr4, %0\n\t"
        "andl %1, %0\n\t"
        "movl %0, %%cr4\n\t"
        "orl %2, %0\n\t"
        "movl %0, %%cr4\n\t"
        : "=&r"(cr4)
        : "r"(~0x80u), "r"(0x80u)
        : "memory"
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

bool jlos_hal_paging_supports_4mb_pages(void)
{
    uint32_t eax, ebx, ecx, edx;
    
    __asm__ __volatile__(
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    return (edx & (1 << 3)) != 0;
}

struct jlos_paging_context *jlos_hal_paging_get_active_context(void)
{
    return s_active_paging_context[jlos_hal_get_cpu_id()];
}

void jlos_hal_paging_set_active_context(struct jlos_paging_context *ctx)
{
    s_active_paging_context[jlos_hal_get_cpu_id()] = ctx;
}

void jlos_hal_paging_enable_global_pages(void)
{
    uint32_t cr4;
    __asm__ __volatile__(
        "movl %%cr4, %0\n\t"
        "orl $0x80, %0\n\t"
        "movl %0, %%cr4\n\t"
        : "=r"(cr4) : : "memory"
    );
}

uint32_t jlos_hal_paging_asid_alloc(void)
{
    return 0;
}

void jlos_hal_paging_asid_free(uint32_t asid)
{
    (void)asid;
}

