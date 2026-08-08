#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include "kernel/idt.h"

void kpanic(const char *reason, registers_t *regs);

void kpanic_page_fault(unsigned long long fault_addr, registers_t *regs);

#endif
