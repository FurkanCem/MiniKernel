#include "kernel/io.h"
#include "kernel/klog.h"
#include "kernel/power.h"

void poweroff(void) {
  unsigned long long flags = irq_save();
  (void)flags;

  klog_write("power: shutting down\n");

  /* QEMU's ACPI PM1 control register. */
  outw(0x604, 0x2000);

  /* Compatibility ports used by QEMU/Bochs firmware variants. */
  outw(0xB004, 0x2000);
  outw(0x4004, 0x3400);

  for (;;) {
    __asm__ volatile("hlt");
  }
}
