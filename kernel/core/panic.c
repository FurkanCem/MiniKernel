#include "kernel/panic.h"
#include "kernel/klog.h"
#include "kernel/video.h"

static void halt_forever(void) {
  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}

static void log_common_regs(registers_t *regs) {
  if (regs == 0)
    return;

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

void kpanic(const char *reason, registers_t *regs) {
  clearwin();
  putstr("*** KERNEL PANIC ***");
  putstr_at(reason, 1);

  klog_write("\n*** KERNEL PANIC: ");
  klog_write(reason);
  klog_write(" ***\n");

  log_common_regs(regs);

  halt_forever();
}

void kpanic_page_fault(unsigned long long fault_addr, registers_t *regs) {
  clearwin();
  putstr("*** KERNEL PANIC ***");
  putstr_at("Page Fault", 1);

  klog_write("\n*** KERNEL PANIC: Page Fault ***\n");
  klog_write("  fault address (CR2): ");
  klog_write_hex(fault_addr);
  klog_write("\n");

  if (regs != 0) {
    unsigned long long ec = regs->error_code;
    klog_write("  cause: ");
    klog_write(ec & 0x1 ? "protection violation" : "page not present");
    klog_write(", ");
    klog_write(ec & 0x2 ? "write" : "read");
    klog_write(", ");
    klog_write(ec & 0x4 ? "user mode" : "supervisor mode");
    if (ec & 0x8)
      klog_write(", reserved bit set in page table (corrupted entry)");
    if (ec & 0x10)
      klog_write(", instruction fetch");
    klog_write("\n");
  }

  log_common_regs(regs);

  halt_forever();
}
