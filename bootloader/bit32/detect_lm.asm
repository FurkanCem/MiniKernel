
[bits 32]

detect_lm_32:
    pushad

    pushfd 
    pop eax

    mov ecx, eax
    xor eax, 1 << 21

    push eax
    popfd

    pushfd
    pop eax

    push ecx
    popfd


    mov eax, 0x80000000             
    cpuid                          

    mov eax, 0x80000001             
    cpuid                          
    test edx, 1 << 29             
    
    popad
    ret


