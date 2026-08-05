#ifndef KERNEL_IRQ_H
#define KERNEL_IRQ_H

#include "kernel/idt.h"

typedef void (*irq_handler_fn)(registers_t *regs);

void irq_register_handler(unsigned char irq, irq_handler_fn handler);

#endif
