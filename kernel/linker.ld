/* The bootloader jumps to a fixed physical address (kernel_start =
   0x8200 in bootloader/boot.asm) expecting _start to be sitting right
   there. Relying on object file link order to guarantee that is
   fragile - it silently broke when the CMake port listed C sources
   before assembly sources. This script makes it explicit instead:
   .text.start (see arch/x86_64/start.asm) is always placed first,
   no matter what order objects are passed to the linker. */

ENTRY(_start)

SECTIONS
{
    . = 0x8200;

    .text : {
        *(.text.start)
        *(.text)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}
