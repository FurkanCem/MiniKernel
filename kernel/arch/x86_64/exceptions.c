#include "kernel/idt.h"
#include "kernel/panic.h"

static const char *exception_names[32] = {"Divide-by-zero",
                                          "Debug",
                                          "NMI",
                                          "Breakpoint",
                                          "Overflow",
                                          "Bound Range Exceeded",
                                          "Invalid Opcode",
                                          "Device Not Available",
                                          "Double Fault",
                                          "Coprocessor Overrun",
                                          "Invalid TSS",
                                          "Segment Not Present",
                                          "Stack-Segment Fault",
                                          "General Protection Fault",
                                          "Page Fault",
                                          "Reserved",
                                          "x87 Floating-Point",
                                          "Alignment Check",
                                          "Machine Check",
                                          "SIMD Floating-Point",
                                          "Virtualization",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved",
                                          "Reserved"};

void isr_handler(registers_t *regs) {
  const char *name = "Unknown Exception";
  if (regs->vector < 32)
    name = exception_names[regs->vector];

  kpanic(name, regs);
}
