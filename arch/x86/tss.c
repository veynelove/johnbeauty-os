#include <arch/x86/tss.h>

void jlos_x86_tss_init(jlos_x86_tss_t *self, uint32_t esp0, uint16_t ss0)
{
    self->back_link = 0;
    self->esp0 = esp0;
    self->ss0 = ss0;
    self->esp1 = 0;
    self->ss1 = 0;
    self->esp2 = 0;
    self->ss2 = 0;
    self->cr3 = 0;
    self->eip = 0;
    self->eflags = 0;
    self->eax = 0;
    self->ecx = 0;
    self->edx = 0;
    self->ebx = 0;
    self->esp = 0;
    self->ebp = 0;
    self->esi = 0;
    self->edi = 0;
    self->es = 0;
    self->cs = 0;
    self->ss = 0;
    self->ds = 0;
    self->fs = 0;
    self->gs = 0;
    self->ldt_segment = 0;
    self->trap_flag = 0;
    self->io_map_base = 104;
}

void jlos_x86_tss_load(jlos_x86_tss_t *self, uint16_t selector)
{
    __asm__ __volatile__("ltr %0" : : "r"(selector));
}
