[bits 16]

; Detects usable/reserved RAM regions via BIOS INT 0x15, EAX=0xE820,
; and stores them for the kernel to read later. This has to run here,
; in real mode, because BIOS interrupts are gone the moment we switch
; to protected mode - there's no way to ask again once we're past
; elevate_bios.
;
; Layout at E820_BUFFER (physical 0x5000, the page right after the
; page tables bit32/init_pt.asm sets up at 0x1000-0x5000):
;   [0x5000]      u32  entry count
;   [0x5004..]    entry_count * 24-byte entries:
;                     u64 base, u64 length, u32 type, u32 extended_attributes
;
; The kernel reads this same physical address directly (see
; kernel/mm/e820.c) since the bootloader identity-maps the first 2MB.

E820_BUFFER      equ 0x5000
E820_ENTRY_SIZE  equ 24
E820_MAX_ENTRIES equ 64

detect_memory_e820:
    pushad
    push es

    xor ax, ax
    mov es, ax

    mov dword [E820_BUFFER], 0     ; entry count starts at 0
    mov edi, E820_BUFFER + 4       ; first entry goes right after the count
    xor ebx, ebx                   ; continuation value - must start at 0

.loop:
    mov eax, 0xE820
    mov ecx, E820_ENTRY_SIZE
    mov edx, 0x534D4150            ; 'SMAP' - required magic for this call
    int 0x15

    jc .done                       ; carry set: error, or list exhausted
    cmp eax, 0x534D4150
    jne .done                      ; BIOS must echo 'SMAP' back in eax

    cmp ecx, 20
    jb .skip_entry                 ; malformed/truncated entry - skip it

    inc dword [E820_BUFFER]
    add edi, E820_ENTRY_SIZE

.skip_entry:
    test ebx, ebx
    jz .done                       ; ebx=0 means that was the last entry

    cmp dword [E820_BUFFER], E820_MAX_ENTRIES
    jae .done                      ; simple safety cap on the buffer

    jmp .loop

.done:
    pop es
    popad
    ret
