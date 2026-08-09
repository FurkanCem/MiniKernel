[bits 64]

%define USER_CODE_SEL 0x1B ; 0x18 | 3
%define USER_DATA_SEL 0x23 ; 0x20 | 3

global enter_usermode
enter_usermode:
    mov ax, USER_DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; SS is set by iretq itself below, not loaded here

    push qword USER_DATA_SEL   ; SS
    push rsi                   ; RSP
    push qword 0x002
    push qword USER_CODE_SEL   ; CS
    push rdi                   ; RIP
    iretq

global user_demo_entry
user_demo_entry:
    mov rax, 1      ; SYS_WRITE_HELLO
    int 0x80

    mov rax, 2      ; SYS_EXIT - does not return
    int 0x80

.spin:              ; unreachable unless SYS_EXIT somehow returns
    jmp .spin
