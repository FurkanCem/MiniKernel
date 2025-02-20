[bits 32]

print_32:
    pushad
    mov edx, vga_start

    print_32_loop:
        cmp byte[esi], 0
        je  print_32_done

        mov al, byte[esi]
        mov ah, style_wb

        mov word[edx], ax

        add esi, 1
        add edx, 2

        jmp print_32_loop

print_32_done:
    popad
    ret
