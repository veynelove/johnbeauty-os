#include <hal/kernel_syscall.h>

jlos_hal_syscall_entry_fn          jlos_hal_syscall_entry          = 0;
jlos_hal_syscall_dispatch_fn       jlos_hal_syscall_dispatch       = 0;
jlos_hal_syscall_resched_check_fn  jlos_hal_syscall_resched_check  = 0;
jlos_hal_syscall_resched_do_fn     jlos_hal_syscall_resched_do     = 0;
