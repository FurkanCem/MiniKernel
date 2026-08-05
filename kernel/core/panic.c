#include "kernel/panic.h"
#include "kernel/klog.h"
#include "kernel/video.h"

void kpanic(const char *reason, registers_t *regs) {
  clearwin();
  putstr("*** KERNEL PANIC ***");
  putstr_at(reason, 1);

  klog_write("\n*** KERNEL PANIC: ");
  klog_write(reason);
  klog_write(" ***\n");

  if (regs != 0) {
    klog_write("  vector:      ");
    klog_write_hex(regs->vector);
    klog_write("\n  error_code:  ");
    klog_write_hex(regs->error_code);
    klog_write("\n  faulting rip:");
    klog_write_hex(regs->rip);
    klog_write("\n  rsp:         ");
    klog_write_hex(regs->rsp);
    klog_write("\n");
  }

  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}
