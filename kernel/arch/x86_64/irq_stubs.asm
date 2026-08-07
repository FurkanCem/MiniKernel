[bits 64]

extern irq_handler

; Hardware IRQs never push an error code, unlike some CPU exceptions -
; fake one with 0 so the stack shape matches registers_t exactly, the
; same way isr.asm does for exceptions.
%macro IRQ 2
global irq%1
irq%1:
    push qword 0
    push qword %2
    jmp irq_common_stub
%endmacro

IRQ 0, 32   ; IRQ0 -> vector 32 (PIT timer)
IRQ 1, 33   ; IRQ1 -> vector 33 (keyboard)

irq_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp        ; pass pointer to registers_t as arg 1
    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16         ; drop vector + fake error code
    iretq
