#include <arch/x86/tss.h>

void jlos_x86_tss_init(jlos_x86_tss_t *self, uint32_t esp0, uint16_t ss0)
{
    self->m_back_link = 0;
    self->m_esp0 = esp0;
    self->m_ss0 = ss0;
    self->m_esp1 = 0;
    self->m_ss1 = 0;
    self->m_esp2 = 0;
    self->m_ss2 = 0;
    self->m_cr3 = 0;
    self->m_eip = 0;
    self->m_eflags = 0;
    self->m_eax = 0;
    self->m_ecx = 0;
    self->m_edx = 0;
    self->m_ebx = 0;
    self->m_esp = 0;
    self->m_ebp = 0;
    self->m_esi = 0;
    self->m_edi = 0;
    self->m_es = 0;
    self->m_cs = 0;
    self->m_ss = 0;
    self->m_ds = 0;
    self->m_fs = 0;
    self->m_gs = 0;
    self->m_ldt_segment = 0;
    self->m_trap_flag = 0;
    self->m_io_map_base = 104;
}

void jlos_x86_tss_load(jlos_x86_tss_t *self, uint16_t selector)
{
    __asm__ __volatile__("ltr %0" : : "r"(selector));
}
