ENTRY(loader)   /* 入口符号 */
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

KERNEL_VIRTAL_BASE = 0x100000;  /* 1mb */

SECTIONS
{
    . = KERNEL_VIRTAL_BASE; /* 为BIOS,VGA等预留内存 */
    .text ALIGN(4K) :
    {
        *(.multiboot)
        . = ALIGN(8K); /* 确保multiboot header在8kb内 */
        *(.text*)
        *(.rodata)
    }
    .data ALIGN(4K) :
    {
        __init_array_start = .;
        KEEP(*( .init_array ));
        KEEP(*(SORT_BY_INIT_PRIORITY( .init_array.*)));
        __init_array_end = .;

        *(.data)
    }
    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
    
    _kernel_end = .;

    /DISCARD/ : /* 丢弃不用节 */
    {
        *(.comment)
    }
}

/*
内存布局示例：
0x000000 ┌─────────────────┐
         │ 实模式内存      │
         │ (0-640KB)       │
0x0A0000 ├─────────────────┤
         │ VGA显存        │
         │ BIOS ROM        │
0x100000 ├─────────────────┤ ← 1MB边界（关键！）
         │ 内核加载区      │ ← 现代操作系统内核
         │                 │

0x00000 - 0x9FFFF  (640KB): 常规内存
0xA0000 - 0xBFFFF  (128KB): VGA文本/图形缓冲区
0xC0000 - 0xDFFFF  (128KB): VGA BIOS/其他ROM
0xE0000 - 0xFFFFF  (128KB): 系统BIOS
0x100000 - ...            : 扩展内存
*/
