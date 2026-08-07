[bits 64]
[extern kernel_main]
[extern _bss_start]
[extern _bss_end]

section .text.start
	global _start
_start:
	mov rdi, _bss_start
	mov rcx, _bss_end
	sub rcx, rdi
	xor al, al
	rep stosb

    	call kernel_main
    	jmp $
