.set MAGIC, 0x1badb002
.set FLAGS, (1<<0 | 1<<1) ; // (1<<0)要求引导程序将内核页对齐到4KB边界，(1<<1)要求引导程序提供内存信息
.set CHECKSUM, -(MAGIC + FLAGS) 

.section .multiboot ; // multiboot头部必须在最前面
.align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

.section .text
.extern john_beauty_main
.extern call_constructors
.global loader

loader:
    cmp $0x2badb002, %eax ; // 0x1BADB002 标识这是一个Multiboot兼容内核, 0x2BADB002 表示引导加载程序是Multiboot兼容的
    jne _stop

    mov $kernel_stack, %esp

    call call_constructors

    push %eax ; // multiboot magic
    push %ebx   ; // multiboot info
    call john_beauty_main

_stop:
    cli ; // 禁用中断
    hlt ; // 停机指令
    jmp _stop

.section .bss
.align 16
.space 4*1024*1024  ; // 4 mi_b
kernel_stack: ; //汇编器两遍扫描, 栈顶地址
