#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "kernel/idt.h"

#define SYS_WRITE_HELLO 1
#define SYS_EXIT 2
#define SYS_WRITE 3
#define SYS_READ 4

void syscall_handler(registers_t *regs);

#endif
