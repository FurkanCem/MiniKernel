#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "kernel/idt.h"

#define SYS_WRITE_HELLO 1 /* ring-3 demo: log a fixed message, no args */
#define SYS_EXIT 2        /* ring-3 demo: terminate the calling thread */

void syscall_handler(registers_t *regs);

#endif
