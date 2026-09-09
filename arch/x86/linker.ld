ENTRY(loader)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

KERNEL_VIRTUAL_BASE = 0xC0000000;

SECTIONS
{
    . = 0x100000;
    __phys_dot = .;

    .boot ALIGN(4K) : AT(__phys_dot)
    {
        _boot_start = .;
        *(.multiboot)
        *(.boot)
        . = ALIGN(4K);
    }
    __phys_dot = LOADADDR(.boot) + SIZEOF(.boot);
    _boot_end = KERNEL_VIRTUAL_BASE + __phys_dot;
    _boot_end_phys = __phys_dot;

    . = KERNEL_VIRTUAL_BASE + __phys_dot;

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
