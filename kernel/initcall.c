#include <kernel/initcall.h>

extern jlos_initcall_fn_t __initcall0_start, __initcall0_end;
extern jlos_initcall_fn_t __initcall1_start, __initcall1_end;
extern jlos_initcall_fn_t __initcall2_start, __initcall2_end;
extern jlos_initcall_fn_t __initcall3_start, __initcall3_end;
extern jlos_initcall_fn_t __initcall4_start, __initcall4_end;

void jlos_do_initcalls(void)
{
    for (jlos_initcall_fn_t *p = &__initcall0_start; p < &__initcall0_end; p++) (*p)();
    for (jlos_initcall_fn_t *p = &__initcall1_start; p < &__initcall1_end; p++) (*p)();
    for (jlos_initcall_fn_t *p = &__initcall2_start; p < &__initcall2_end; p++) (*p)();
    for (jlos_initcall_fn_t *p = &__initcall3_start; p < &__initcall3_end; p++) (*p)();
    for (jlos_initcall_fn_t *p = &__initcall4_start; p < &__initcall4_end; p++) (*p)();
}
