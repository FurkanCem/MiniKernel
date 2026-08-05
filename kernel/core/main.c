#include "kernel/idt.h"
#include "kernel/keyboard.h"
#include "kernel/klog.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "kernel/video.h"

int kernel_main(void) {
  clearwin();

  klog_init();
  klog_write("MiniKernel: log online\n");

  idt_init();
  klog_write("MiniKernel: IDT loaded\n");

  pic_remap(0x20, 0x28);
  timer_driver_init(
      100); /* 100 Hz tick instead of the PIT's default ~18.2 Hz */
  keyboard_driver_init();

  pic_unmask_irq(0); /* IRQ0: PIT timer */
  pic_unmask_irq(1); /* IRQ1: keyboard */
  __asm__ volatile(
      "sti"); /* interrupts were off since boot - turn them on now */

  klog_write("MiniKernel: PIC remapped, timer + keyboard IRQs live\n");

  const char *welcome_msg = "Working kernel";
  putstr(welcome_msg);
  putstr_at("IDT loaded", 1);

  // __asm__ volatile ("int3");
  // volatile int z = 0; volatile int y = 1 / z;

  return 0;
}
