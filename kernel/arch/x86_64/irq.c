#include "kernel/irq.h"
#include "kernel/pic.h"

#define MAX_IRQS 16
static irq_handler_fn handlers[MAX_IRQS] = {0};

void irq_register_handler(unsigned char irq, irq_handler_fn handler) {
  if (irq < MAX_IRQS)
    handlers[irq] = handler;
}

void irq_handler(registers_t *regs) {
  unsigned char irq = (unsigned char)(regs->vector - 32);

  if (irq < MAX_IRQS && handlers[irq] != 0) {
    handlers[irq](regs);
  }

  pic_send_eoi(irq);
}
