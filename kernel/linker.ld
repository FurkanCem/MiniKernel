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
        *(.data.rel*)   /* GCC sometimes emits this for const pointer
                            initializers even without -fPIC; without
                            an explicit rule here it becomes an orphan
                            section that ld auto-places, which works
                            but isn't something to rely on silently. */
    }

    /* Our bootloader is a flat sector loader with no concept of
       "allocate this much memory and zero it" - it only ever copies
       exactly the bytes that exist in the raw kernel binary file.
       .bss has no file contents by definition (that's the whole
       point of .bss), so nothing was ever zeroing this region before
       start.asm now does it explicitly using these two symbols. Any
       uninitialized global relied on this being zero and silently
       wasn't, until now. */
    _bss_start = .;
    .bss : {
        *(.bss)
        *(COMMON)
    }
    _bss_end = .;

    _kernel_end = .;

    /* Nothing here reads DWARF unwind tables or needs a linker
       comment string in the final image - drop them rather than
       auto-place them and grow the binary for no reason. */
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.*)
    }
}
