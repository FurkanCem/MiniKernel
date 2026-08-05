#include "kernel/idt.h"

typedef struct __attribute__((packed)) {
  unsigned short offset_low;
  unsigned short selector;
  unsigned char ist;
  unsigned char type_attr;
  unsigned short offset_mid;
  unsigned int offset_high;
  unsigned int zero;
} idt_entry_t;

typedef struct __attribute__((packed)) {
  unsigned short limit;
  unsigned long long base;
} idt_ptr_t;

#define IDT_ENTRIES 256
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

#define KERNEL_CODE_SEG 0x08

/* present=1, ring=00, type=1110 -> 64-bit interrupt gate */
#define IDT_GATE_INTERRUPT 0x8E

static void idt_set_gate(int n, unsigned long long handler) {
  idt[n].offset_low = (unsigned short)(handler & 0xFFFF);
  idt[n].selector = KERNEL_CODE_SEG;
  idt[n].ist = 0;
  idt[n].type_attr = IDT_GATE_INTERRUPT;
  idt[n].offset_mid = (unsigned short)((handler >> 16) & 0xFFFF);
  idt[n].offset_high = (unsigned int)((handler >> 32) & 0xFFFFFFFF);
  idt[n].zero = 0;
}

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* IRQ0 (PIT timer) and IRQ1 (keyboard) stubs, defined in irq_stubs.asm. */
extern void irq0(void);
extern void irq1(void);

void idt_init(void) {
  idt_set_gate(0, (unsigned long long)isr0);
  idt_set_gate(1, (unsigned long long)isr1);
  idt_set_gate(2, (unsigned long long)isr2);
  idt_set_gate(3, (unsigned long long)isr3);
  idt_set_gate(4, (unsigned long long)isr4);
  idt_set_gate(5, (unsigned long long)isr5);
  idt_set_gate(6, (unsigned long long)isr6);
  idt_set_gate(7, (unsigned long long)isr7);
  idt_set_gate(8, (unsigned long long)isr8);
  idt_set_gate(9, (unsigned long long)isr9);
  idt_set_gate(10, (unsigned long long)isr10);
  idt_set_gate(11, (unsigned long long)isr11);
  idt_set_gate(12, (unsigned long long)isr12);
  idt_set_gate(13, (unsigned long long)isr13);
  idt_set_gate(14, (unsigned long long)isr14);
  idt_set_gate(15, (unsigned long long)isr15);
  idt_set_gate(16, (unsigned long long)isr16);
  idt_set_gate(17, (unsigned long long)isr17);
  idt_set_gate(18, (unsigned long long)isr18);
  idt_set_gate(19, (unsigned long long)isr19);
  idt_set_gate(20, (unsigned long long)isr20);
  idt_set_gate(21, (unsigned long long)isr21);
  idt_set_gate(22, (unsigned long long)isr22);
  idt_set_gate(23, (unsigned long long)isr23);
  idt_set_gate(24, (unsigned long long)isr24);
  idt_set_gate(25, (unsigned long long)isr25);
  idt_set_gate(26, (unsigned long long)isr26);
  idt_set_gate(27, (unsigned long long)isr27);
  idt_set_gate(28, (unsigned long long)isr28);
  idt_set_gate(29, (unsigned long long)isr29);
  idt_set_gate(30, (unsigned long long)isr30);
  idt_set_gate(31, (unsigned long long)isr31);

  /* Hardware IRQs land here once pic_remap(0x20, ...) has moved them
     out of the way of the exception vectors above. */
  idt_set_gate(32, (unsigned long long)irq0); /* IRQ0: PIT timer */
  idt_set_gate(33, (unsigned long long)irq1); /* IRQ1: keyboard  */

  idt_ptr.limit = sizeof(idt) - 1;
  idt_ptr.base = (unsigned long long)&idt;

  __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
