#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

/* Registers the IRQ1 handler. Call after idt_init() and pic_remap(),
   before pic_unmask_irq(1). */
void keyboard_driver_init(void);

int kbd_read_char(char *out);

#endif
