#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include "kernel/idt.h"

void kpanic(const char *reason, registers_t *regs);

#endif
