#include "kernel/keyboard.h"
#include "kernel/io.h"
#include "kernel/irq.h"
#include "kernel/klog.h"

#define KEYBOARD_DATA_PORT 0x60

static void keyboard_irq_handler(registers_t *regs) {
  (void)regs;

  unsigned char scancode = inb(KEYBOARD_DATA_PORT);
  klog_write("keyboard scancode: ");
  klog_write_hex(scancode);
  klog_write("\n");
}

void keyboard_driver_init(void) {
  irq_register_handler(1, keyboard_irq_handler);
}
