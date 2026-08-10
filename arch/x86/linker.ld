ENTRY(loader)   /* loader 标号在 .boot 段，VMA 为物理地址，GRUB 未开分页时能正确跳转 */
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

KERNEL_VIRTUAL_BASE = 0xC0000000;

/* ==========================================================================
 * 核心不变式 (CRITICAL INVARIANT)：
 * 高半核 .text / .rodata / .data / .bss 中的每一字节必须满足：
 *     VIRTUAL_ADDR(byte) - PHYSICAL_ADDR(byte) == KERNEL_VIRTUAL_BASE
 * 因为启动页表 PSE 做的就是  VA 0xC0000000+off -> PA 0+off (4MB 大页).
 * 任何 VMA/LMA 多出的对齐空洞都会破坏这个等式，导致 VA 映射的 PA 上没有代码！
 *
 * 实现方法：用独立的 __phys_dot 跟踪"物理加载地址 LMA"，用 '.' 跟踪 VMA。
 *   __phys_dot  只在物理内存里向前推进 (LMA)。
 *   '.'         永远 = KERNEL_VIRTUAL_BASE + __phys_dot  (VMA)。
 * 所以任何字节只要放进一个 section，它的 V 就精确等于 KVB + L。
 * ========================================================================== */
SECTIONS
{
    /* ---------- .multiboot + .boot：V == L == 物理，1MB 边界起 ---------- */
    . = 0x100000;
    __phys_dot = .;             /* 物理加载计数器，和 VMA 一致 */

    .boot ALIGN(4K) : AT(__phys_dot)
    {
        _boot_start = .;
        *(.multiboot)
        *(.boot)
        . = ALIGN(4K);
    }
    __phys_dot = LOADADDR(.boot) + SIZEOF(.boot);   /* 物理推进到 .boot 末尾 */
    _boot_end = KERNEL_VIRTUAL_BASE + __phys_dot;   /* _boot_end 也提供一份 VA 版 */
    _boot_end_phys = __phys_dot;                    /* 同时提供物理版符号 */

    /* ============ 高半核：强制  V(.start) = KVB + L(.start)  ============ */
    . = KERNEL_VIRTUAL_BASE + __phys_dot;           /* 设定 VMA 计数器起点 */

    .text : AT(__phys_dot)
    {
        _text_start = .;
        *(.text*)
        . = ALIGN(4K);
        _text_end = .;
    }
    __phys_dot = LOADADDR(.text) + SIZEOF(.text);

    .rodata : AT(__phys_dot)
    {
        _rodata_start = .;
        *(.rodata*)
        . = ALIGN(4K);
        _rodata_end = .;
    }
    __phys_dot = LOADADDR(.rodata) + SIZEOF(.rodata);

    .data : AT(__phys_dot)
    {
        _data_start = .;
        __init_array_start = .;
        KEEP(*( .init_array ));
        KEEP(*(SORT_BY_INIT_PRIORITY( .init_array.*)));
        __init_array_end = .;
        *(.data)
        . = ALIGN(4K);
        _data_end = .;
    }
    __phys_dot = LOADADDR(.data) + SIZEOF(.data);

    .bss : AT(__phys_dot)
    {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4K);
        _bss_end = .;
    }
    __phys_dot = LOADADDR(.bss) + SIZEOF(.bss);

    . = ALIGN(4K);
    _kernel_end = .;
    _kernel_end_phys = __phys_dot;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.got)
        *(.got.plt)
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

0x00000 - 0x9FFFF  (640KB): 常规内存
0xA0000 - 0xBFFFF  (128KB): VGA文本/图形缓冲区
0xC0000 - 0xDFFFF  (128KB): VGA BIOS/其他ROM
0xE0000 - 0xFFFFF  (128KB): 系统BIOS
0x100000 - ...            : 扩展内存
*/
