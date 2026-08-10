ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(4096)
    {
        *(.text._start)
        *(.text*)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata*)
    }

    .data : ALIGN(4096)
    {
        *(.data*)
        *(.data.rel*)
    }

    .bss : ALIGN(4096)
    {
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note.*)
    }
}
