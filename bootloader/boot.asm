[org 0x7C00]

jmp begin_real

kernel_size db 0
begin_real:

[bits 16]
mov bp, 0x0500
mov sp, bp
mov byte[boot_drive], dl
mov bx, msg_hello_world
call print_bios

mov bx, 0x0002
mov cx, [kernel_size]
add cx, 2
mov dx, 0x7E00

call load_bios

call elevate_bios

bootsector_hold:
jmp $           
%include "real/print.asm"
%include "real/print_hex.asm"
%include "real/load.asm"
%include "real/gdt.asm"
%include "real/elevate.asm"
msg_hello_world:                db `\r\nHello World, from the BIOS!\r\n`, 0
boot_drive:                     db 0x00
times 510 - ($ - $$) db 0x00

dw 0xAA55
bootsector_extended:
begin_32:

[bits 32]
call clear_32
call detect_lm_32
mov esi, protected_alert
call print_32
call init_pt_32

call elevate_32

jmp $     
%include "bit32/clear.asm"
%include "bit32/print.asm"
%include "bit32/detect_lm.asm"
%include "bit32/init_pt.asm"
%include "bit32/gdt.asm"
%include "bit32/elevate.asm"
vga_start:                  equ 0x000B8000
vga_extent:                 equ 80 * 25 * 2   
style_wb:                   equ 0x0F
protected_alert:                 db `64-bit long mode supported`, 0
times 512 - ($ - bootsector_extended) db 0x00
begin_long:

[bits 64]

mov rdi, style_blue
call clear_long

mov rdi, style_blue
mov rsi, long_mode_note
call print_long

call kernel_start

jmp $

%include "long/clear.asm"
%include "long/print.asm"

kernel_start:                   equ 0x8200              
long_mode_note:                 db `Now running in fully-enabled, 64-bit long mode!`, 0
style_blue:                     equ 0x1F

times 512 - ($ - begin_long) db 0x00
