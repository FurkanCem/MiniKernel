[bits 64]
[extern kernel_main]

section .text.start
	global _start
_start:
    	call kernel_main
    	jmp $
